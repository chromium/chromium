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
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace device_signals {

namespace {

constexpr size_t kMaxZtaFileSize = 32 * 1024;
constexpr char kAgentIdPropertyKey[] = "sub";

void OnPayloadParsed(
    base::OnceCallback<void(absl::optional<CrowdStrikeSignals>)> callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    LogCrowdStrikeParsingError(SignalsParsingError::kJsonParsingFailed);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  const std::string* agent_id = result->FindStringPath(kAgentIdPropertyKey);
  if (!agent_id) {
    LogCrowdStrikeParsingError(SignalsParsingError::kMissingRequiredProperty);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  CrowdStrikeSignals identifiers;
  identifiers.agent_id = *agent_id;
  std::move(callback).Run(identifiers);
}

}  // namespace

class CrowdStrikeClientImpl : public CrowdStrikeClient {
 public:
  explicit CrowdStrikeClientImpl(const base::FilePath& zta_file_path);

  ~CrowdStrikeClientImpl() override;

  // CrowdStrikeClient:
  void GetIdentifiers(
      base::OnceCallback<void(absl::optional<CrowdStrikeSignals>)> callback)
      override;

 private:
  const base::FilePath zta_file_path_;
  data_decoder::DataDecoder data_decoder_;
};

// static
std::unique_ptr<CrowdStrikeClient> CrowdStrikeClient::Create() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return std::make_unique<CrowdStrikeClientImpl>(GetCrowdStrikeZtaFilePath());
#else
  NOTREACHED();
  return nullptr;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

std::unique_ptr<CrowdStrikeClient> CrowdStrikeClient::CreateForTesting(
    const base::FilePath& zta_file_path) {
  return std::make_unique<CrowdStrikeClientImpl>(zta_file_path);
}

CrowdStrikeClientImpl::CrowdStrikeClientImpl(
    const base::FilePath& zta_file_path)
    : zta_file_path_(zta_file_path) {}

CrowdStrikeClientImpl::~CrowdStrikeClientImpl() = default;

void CrowdStrikeClientImpl::GetIdentifiers(
    base::OnceCallback<void(absl::optional<CrowdStrikeSignals>)> callback) {
  if (!base::PathExists(zta_file_path_)) {
    // Not finding a file is a supported use-case (not an error).
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(zta_file_path_, &file_content,
                                         kMaxZtaFileSize)) {
    LogCrowdStrikeParsingError(SignalsParsingError::kHitMaxDataSize);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  if (file_content.empty()) {
    // Having an empty file is a supported use-case (not an error).
    std::move(callback).Run(absl::nullopt);
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
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::string json_payload;
  if (!base::Base64UrlDecode(jwt_sections[1],
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &json_payload)) {
    LogCrowdStrikeParsingError(SignalsParsingError::kBase64DecodingFailed);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // Parse the JSON payload in a child process.
  data_decoder_.ParseJson(
      json_payload, base::BindOnce(&OnPayloadParsed, std::move(callback)));
}

}  // namespace device_signals
