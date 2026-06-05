// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/crowdstrike_client.h"

#include <memory>

#include "base/base64url.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/cached_signal.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals {

using SignalsCallback =
    base::OnceCallback<void(std::optional<CrowdStrikeSignals>,
                            std::optional<SignalCollectionError>)>;

namespace {

constexpr int kCacheExpiryInHours = 1;
constexpr size_t kMaxZtaFileSize = 32 * 1024;

constexpr char kAgentIdJwtPropertyKey[] = "sub";
constexpr char kCustomerIdJwtPropertyKey[] = "cid";

// Helper struct to return the results of fetching Crowdstrike agent information
// across sequences.
struct FetchResult {
  std::optional<CrowdStrikeSignals> signals;
  // Note that both `signals` and `error` may be set, if there was an error
  // retrieving the information from the data.zta file but the platform-specific
  // fallback was successful.
  std::optional<SignalCollectionError> error;
  // True iff the signals were retrieved from the data.zta file.
  bool is_data_file_source = false;
};

// Core logic of getting the CrowdStrike agent information. Extracted into a
// function in the anonymous namespace to have it run in a background thread.
// `zta_file_path` points to the data.zta file. Returns the CrowdStrikeSignals
// extracted from the data.zta file on success, or a `SignalsCollectionError`
// otherwise.
base::expected<std::optional<CrowdStrikeSignals>, SignalCollectionError>
GetSignalsFromZtaFile(const base::FilePath& zta_file_path) {
  if (!base::PathExists(zta_file_path)) {
    // Not finding a file is a supported use-case (not an error).
    return std::nullopt;
  }
  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(zta_file_path, &file_content,
                                         kMaxZtaFileSize)) {
    LogCrowdStrikeParsingError(SignalsParsingError::kHitMaxDataSize);
    return base::unexpected(SignalCollectionError::kParsingFailed);
  }

  if (file_content.empty()) {
    // Having an empty file is a supported use-case (not an error).
    return std::nullopt;
  }

  // A valid ZTA file represents a JWT. For parsing out the identifiers, only
  // the payload section is relevant. More information on JWTs here:
  // https://en.wikipedia.org/wiki/JSON_Web_Token
  std::vector<std::string> jwt_sections = base::SplitString(
      file_content, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (jwt_sections.size() != 3) {
    // A JWT payload must have three sections.
    LogCrowdStrikeParsingError(SignalsParsingError::kDataMalformed);
    return base::unexpected(SignalCollectionError::kUnexpectedValue);
  }

  std::string json_payload;
  if (!base::Base64UrlDecode(jwt_sections[1],
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &json_payload)) {
    LogCrowdStrikeParsingError(SignalsParsingError::kBase64DecodingFailed);
    return base::unexpected(SignalCollectionError::kParsingFailed);
  }

  auto result_dict =
      base::JSONReader::ReadDict(json_payload, base::JSON_PARSE_RFC);
  if (!result_dict.has_value()) {
    LogCrowdStrikeParsingError(SignalsParsingError::kJsonParsingFailed);
    return base::unexpected(SignalCollectionError::kParsingFailed);
  }

  const std::string* agent_id = result_dict->FindString(kAgentIdJwtPropertyKey);
  if (!agent_id) {
    LogCrowdStrikeParsingError(SignalsParsingError::kMissingRequiredProperty);
    return base::unexpected(SignalCollectionError::kParsingFailed);
  }

  CrowdStrikeSignals identifiers;
  identifiers.agent_id = *agent_id;

  const std::string* customer_id =
      result_dict->FindString(kCustomerIdJwtPropertyKey);
  if (customer_id) {
    identifiers.customer_id = *customer_id;
  }

  return identifiers;
}

// Called on a background sequence; retrieves signals from the ZTA file if
// possible, falling back to platform-specific mechanisms on failure.
FetchResult FetchSignalsBackground(const base::FilePath& zta_file_path) {
  FetchResult result;
  auto zta_result = GetSignalsFromZtaFile(zta_file_path);
  if (zta_result.has_value()) {
    if (zta_result.value()) {
      result.signals = zta_result.value();
      result.is_data_file_source = true;
    } else {
      result.signals = GetCrowdStrikeSignals();
    }
  } else {
    result.error = zta_result.error();
    result.signals = GetCrowdStrikeSignals();
  }
  return result;
}

}  // namespace

class CrowdStrikeClientImpl : public CrowdStrikeClient {
 public:
  explicit CrowdStrikeClientImpl(const base::FilePath& zta_file_path);
  ~CrowdStrikeClientImpl() override;

  // CrowdStrikeClient:
  void GetIdentifiers(SignalsCallback callback) override;

 private:
  // Callback invoked after the background fetch completes. Handles caching
  // and invoking the original caller's `callback`.
  void OnSignalsFetched(SignalsCallback callback, FetchResult result);

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath zta_file_path_;
  CachedSignal<CrowdStrikeSignals> cached_signals_;

  base::WeakPtrFactory<CrowdStrikeClientImpl> weak_ptr_factory_{this};
};

// static
std::unique_ptr<CrowdStrikeClient> CrowdStrikeClient::Create() {
  return std::make_unique<CrowdStrikeClientImpl>(GetCrowdStrikeZtaFilePath());
}

std::unique_ptr<CrowdStrikeClient> CrowdStrikeClient::CreateForTesting(
    const base::FilePath& zta_file_path) {
  return std::make_unique<CrowdStrikeClientImpl>(zta_file_path);
}

CrowdStrikeClientImpl::CrowdStrikeClientImpl(
    const base::FilePath& zta_file_path)
    : zta_file_path_(zta_file_path),
      cached_signals_(base::Hours(kCacheExpiryInHours)) {}

CrowdStrikeClientImpl::~CrowdStrikeClientImpl() = default;

void CrowdStrikeClientImpl::GetIdentifiers(SignalsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (zta_file_path_.empty()) {
    std::move(callback).Run(/*signals=*/std::nullopt, /*error=*/std::nullopt);
    return;
  }
  const auto& cached_values = cached_signals_.Get();
  if (cached_values) {
    std::move(callback).Run(cached_values.value(), /*error=*/std::nullopt);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&FetchSignalsBackground, zta_file_path_),
      base::BindOnce(&CrowdStrikeClientImpl::OnSignalsFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrowdStrikeClientImpl::OnSignalsFetched(SignalsCallback callback,
                                             FetchResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.signals) {
    base::UmaHistogramBoolean(
        "Enterprise.DeviceSignals.Collection.CrowdStrike.FromDataFile",
        result.is_data_file_source);
    cached_signals_.Set(result.signals.value());
  }

  std::move(callback).Run(std::move(result.signals), result.error);
}

}  // namespace device_signals
