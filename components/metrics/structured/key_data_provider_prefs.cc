// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/key_data_provider_prefs.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "components/metrics/structured/key_data_prefs_delegate.h"
#include "components/metrics/structured/structured_metrics_validator.h"

namespace metrics::structured {

KeyDataProviderPrefs::KeyDataProviderPrefs(PrefService* local_state,
                                           std::string_view pref_name)
    : key_data_(
          std::make_unique<KeyDataPrefsDelegate>(local_state, pref_name)) {}

KeyDataProviderPrefs::~KeyDataProviderPrefs() = default;

bool KeyDataProviderPrefs::IsReady() {
  return true;
}

std::optional<uint64_t> KeyDataProviderPrefs::GetId(
    const std::string& project_name) {
  // Validates the project. If valid, retrieve the metadata associated
  // with the event.
  const auto* project_validator =
      validator::Validators::Get()->GetProjectValidator(project_name);

  if (!project_validator) {
    return std::nullopt;
  }
  return key_data_.Id(project_validator->project_hash(),
                      base::Days(project_validator->key_rotation_period()));
}

KeyData* KeyDataProviderPrefs::GetKeyData(const std::string& project_name) {
  return &key_data_;
}

void KeyDataProviderPrefs::Purge() {
  key_data_.Purge();
}

}  // namespace metrics::structured
