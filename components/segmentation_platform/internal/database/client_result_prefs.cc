// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/client_result_prefs.h"

#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/internal/constants.h"

namespace segmentation_platform {

ClientResultPrefs::ClientResultPrefs(PrefService* pref_service)
    : prefs_(pref_service) {}

void ClientResultPrefs::SaveClientResultToPrefs(
    const std::string& client_key,
    std::optional<proto::ClientResult> client_result) {
  InitializeIfNeeded();

  if (client_result.has_value()) {
    (*cached_results_.mutable_client_result_map())[client_key] =
        std::move(client_result.value());
  } else {
    // Erasing the entry if the new `client_result` is null.
    auto client_result_iter =
        cached_results_.client_result_map().find(client_key);
    if (client_result_iter != cached_results_.client_result_map().end()) {
      cached_results_.mutable_client_result_map()->erase(client_key);
    }
  }
  std::string output = base::Base64Encode(cached_results_.SerializeAsString());
  prefs_->SetString(kSegmentationClientResultPrefs, output);
}

const proto::ClientResult* ClientResultPrefs::ReadClientResultFromPrefs(
    const std::string& client_key) {
  InitializeIfNeeded();

  const auto& it = cached_results_.client_result_map().find(client_key);
  if (it != cached_results_.client_result_map().end()) {
    return &it->second;
  }
  return nullptr;
}

void ClientResultPrefs::InitializeIfNeeded() {
  if (initialized_) {
    return;
  }

  initialized_ = true;
  auto decoded_client_results =
      base::Base64Decode(prefs_->GetString(kSegmentationClientResultPrefs));
  const std::string& decoded_client_results_as_string =
      decoded_client_results.has_value()
          ? std::string(decoded_client_results.value().begin(),
                        decoded_client_results.value().end())
          : std::string();

  cached_results_.ParseFromString(decoded_client_results_as_string);
}

}  // namespace segmentation_platform
