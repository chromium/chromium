// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/client_result_prefs.h"

#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/internal/constants.h"

namespace segmentation_platform {

ClientResultPrefs::ClientResultPrefs(PrefService* pref_service)
    : prefs_(pref_service) {}

void ClientResultPrefs::SaveClientResultToPrefs(
    const std::string& client_key,
    const absl::optional<proto::ClientResult>& client_result) {
  proto::ClientResults client_results;

  auto decoded_client_results =
      base::Base64Decode(prefs_->GetString(kSegmentationClientResultPrefs));
  const std::string& decoded_client_results_as_string =
      decoded_client_results.has_value()
          ? std::string(decoded_client_results.value().begin(),
                        decoded_client_results.value().end())
          : std::string();

  client_results.ParseFromString(decoded_client_results_as_string);
  if (client_result.has_value()) {
    (*client_results.mutable_client_result_map())[client_key] =
        client_result.value();
  } else {
    // Erasing the entry if the new `client_result` is null.
    auto client_result_iter =
        client_results.client_result_map().find(client_key);
    if (client_result_iter != client_results.client_result_map().end()) {
      client_results.mutable_client_result_map()->erase(client_key);
    }
  }
  std::string output;
  base::Base64Encode(client_results.SerializeAsString(), &output);
  prefs_->SetString(kSegmentationClientResultPrefs, output);
}

absl::optional<proto::ClientResult>
ClientResultPrefs::ReadClientResultFromPrefs(const std::string& client_key) {
  proto::ClientResults client_results;

  auto decoded_client_results =
      base::Base64Decode(prefs_->GetString(kSegmentationClientResultPrefs));
  const std::string& decoded_client_results_as_string =
      decoded_client_results.has_value()
          ? std::string(decoded_client_results.value().begin(),
                        decoded_client_results.value().end())
          : std::string();

  client_results.ParseFromString(decoded_client_results_as_string);
  if (client_results.client_result_map().contains(client_key)) {
    return client_results.client_result_map().at(client_key);
  }
  return absl::nullopt;
}

}  // namespace segmentation_platform