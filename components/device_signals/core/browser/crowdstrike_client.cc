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
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/cached_signal.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace device_signals {

using SignalsCallback =
    base::OnceCallback<void(std::optional<CrowdStrikeSignals>,
                            std::optional<SignalCollectionError>)>;

namespace {

constexpr int kCacheExpiryInHours = 1;
constexpr size_t kMaxZtaFileSize = 32 * 1024;

constexpr char kAgentIdJwtPropertyKey[] = "sub";
constexpr char kCustomerIdJwtPropertyKey[] = "cid";

// Core logic of getting the CrowdStrike agent information. Extracted into
// a function in the anonymous namespace to have it run in a background
// thread. `zta_file_path` points to the data.zta file. `json_decode_callback`
// can be used to decode JSON values out-of-process and then lead into invoking
// the final callback. `results_callback` is the final callback which ultimately
// returns the collected signals to the caller.
void GetZtaJwtPayload(
    const base::FilePath& zta_file_path,
    base::OnceCallback<void(const std::string&, SignalsCallback)>
        json_decode_callback,
    SignalsCallback results_callback) {
  if (!base::PathExists(zta_file_path)) {
    // Not finding a file is a supported use-case (not an error).
    std::move(results_callback).Run(std::nullopt, std::nullopt);
    return;
  }
  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(zta_file_path, &file_content,
                                         kMaxZtaFileSize)) {
    LogCrowdStrikeParsingError(SignalsParsingError::kHitMaxDataSize);
    std::move(results_callback)
        .Run(std::nullopt, SignalCollectionError::kParsingFailed);
    return;
  }

  if (file_content.empty()) {
    // Having an empty file is a supported use-case (not an error).
    std::move(results_callback).Run(std::nullopt, std::nullopt);
    return;
  }

  // A valid ZTA file represents a JWT. For parsing out the identifiers, only
  // the payload section is relevant. More information on JWTs here:
  // https://en.wikipedia.org/wiki/JSON_Web_Token
  std::vector<std::string> jwt_sections = base::SplitString(
      file_content, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (jwt_sections.size() != 3) {
    // A JWT payload must have three sections.
    LogCrowdStrikeParsingError(SignalsParsingError::kDataMalformed);
    std::move(results_callback)
        .Run(std::nullopt, SignalCollectionError::kUnexpectedValue);
    return;
  }

  std::string json_payload;
  if (!base::Base64UrlDecode(jwt_sections[1],
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &json_payload)) {
    LogCrowdStrikeParsingError(SignalsParsingError::kBase64DecodingFailed);
    std::move(results_callback)
        .Run(std::nullopt, SignalCollectionError::kParsingFailed);
    return;
  }

  std::move(json_decode_callback)
      .Run(json_payload, std::move(results_callback));
}

void OnStaticSignalsRetrieved(SignalsCallback callback,
                              std::optional<SignalCollectionError> error,
                              std::optional<CrowdStrikeSignals> signals) {
  // Forward the unexpected `error` to make sure it is captured in the metrics.
  std::move(callback).Run(signals, error);
}

}  // namespace

class CrowdStrikeClientImpl : public CrowdStrikeClient {
 public:
  explicit CrowdStrikeClientImpl(const base::FilePath& zta_file_path);
  ~CrowdStrikeClientImpl() override;

  // CrowdStrikeClient:
  void GetIdentifiers(SignalsCallback callback) override;

 private:
  // Delegated the JSON decoding of `json_content` to a out-of-process utility.
  // Will invoke OnPayloadParsed with the result, while forwarding `callback`.
  void DecodeJson(const std::string& json_content, SignalsCallback callback);

  // Invoked after decoding some JSON content with `result`. That result is
  // then parsed for the required signals. Then, `callback` is invoked with
  // any signals that were found.
  void OnPayloadParsed(SignalsCallback callback,
                       data_decoder::DataDecoder::ValueOrError result);

  // Final function to be called in this flow with `signals` containing any
  // value that was successfully found. This function will set the cache and
  // then invoke the original caller's `callback`.
  void OnSignalsRetrieved(SignalsCallback callback,
                          std::optional<CrowdStrikeSignals> signals,
                          std::optional<SignalCollectionError> error);

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath zta_file_path_;
  data_decoder::DataDecoder data_decoder_;
  CachedSignal<CrowdStrikeSignals> cached_signals_;

  base::WeakPtrFactory<CrowdStrikeClientImpl> weak_ptr_factory_{this};
};

// static
std::unique_ptr<CrowdStrikeClient> CrowdStrikeClient::Create() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return std::make_unique<CrowdStrikeClientImpl>(GetCrowdStrikeZtaFilePath());
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
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
  const auto& cached_values = cached_signals_.Get();
  if (cached_values) {
    std::move(callback).Run(cached_values.value(), /*error=*/std::nullopt);
    return;
  }

  base::OnceCallback<void(const std::string&, SignalsCallback)>
      json_decode_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
          &CrowdStrikeClientImpl::DecodeJson, weak_ptr_factory_.GetWeakPtr()));

  SignalsCallback result_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&CrowdStrikeClientImpl::OnSignalsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetZtaJwtPayload, zta_file_path_,
                     std::move(json_decode_callback),
                     std::move(result_callback)));
}

void CrowdStrikeClientImpl::DecodeJson(const std::string& json_content,
                                       SignalsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Parse the JSON content in a child process.
  data_decoder_.ParseJson(
      json_content,
      base::BindOnce(&CrowdStrikeClientImpl::OnPayloadParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrowdStrikeClientImpl::OnPayloadParsed(
    SignalsCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    LogCrowdStrikeParsingError(SignalsParsingError::kJsonParsingFailed);
    std::move(callback).Run(std::nullopt,
                            SignalCollectionError::kParsingFailed);
    return;
  }

  const base::Value::Dict& result_dict = result->GetDict();
  const std::string* agent_id = result_dict.FindString(kAgentIdJwtPropertyKey);
  if (!agent_id) {
    LogCrowdStrikeParsingError(SignalsParsingError::kMissingRequiredProperty);
    std::move(callback).Run(std::nullopt,
                            SignalCollectionError::kParsingFailed);
    return;
  }

  CrowdStrikeSignals identifiers;
  identifiers.agent_id = *agent_id;

  const std::string* customer_id =
      result_dict.FindString(kCustomerIdJwtPropertyKey);
  if (customer_id) {
    identifiers.customer_id = *customer_id;
  }

  std::move(callback).Run(identifiers, /*error=*/std::nullopt);
}

void CrowdStrikeClientImpl::OnSignalsRetrieved(
    SignalsCallback callback,
    std::optional<CrowdStrikeSignals> signals,
    std::optional<SignalCollectionError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!signals) {
    // If signals could not be retrieved via the ZTA file, then fallback to
    // some other platform-specific mechanism. However, do not cache that
    // value as it is inexpensive to retrieve and the ZTA file is preferred.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&GetCrowdStrikeSignals),
        base::BindOnce(&OnStaticSignalsRetrieved, std::move(callback),
                       std::move(error)));
    return;
  }

  cached_signals_.Set(signals.value());
  std::move(callback).Run(std::move(signals), error);
}

}  // namespace device_signals
