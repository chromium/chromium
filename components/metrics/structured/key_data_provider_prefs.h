// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_PREFS_H_
#define COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_PREFS_H_

#include <optional>
#include <string_view>

#include "components/metrics/structured/lib/key_data.h"
#include "components/metrics/structured/lib/key_data_provider.h"
#include "components/prefs/pref_service.h"

namespace metrics::structured {

// KeyDataProvider implementation that stores the keys in a preferences.
class KeyDataProviderPrefs : public KeyDataProvider {
 public:
  KeyDataProviderPrefs(PrefService* local_state, std::string_view pref_name);

  ~KeyDataProviderPrefs() override;

  // KeyDataProvider:
  bool IsReady() override;
  std::optional<uint64_t> GetId(const std::string& project_name) override;
  KeyData* GetKeyData(const std::string& project_name) override;
  void Purge() override;

 private:
  KeyData key_data_;
};
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_PREFS_H_
