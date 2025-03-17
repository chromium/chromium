// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SETTINGS_FAKE_CROS_SETTINGS_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SETTINGS_FAKE_CROS_SETTINGS_PROVIDER_H_

#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/prefs/pref_value_map.h"

namespace ash {

// Fake implementation of CrosSettingsProvider for testing.
class FakeCrosSettingsProvider : public CrosSettingsProvider {
 public:
  explicit FakeCrosSettingsProvider(const NotifyObserversCallback& notify_cb);
  FakeCrosSettingsProvider(const FakeCrosSettingsProvider&) = delete;
  FakeCrosSettingsProvider& operator=(const FakeCrosSettingsProvider&) = delete;
  ~FakeCrosSettingsProvider() override;

  // CrosSettingsProvider:
  const base::Value* Get(std::string_view path) const override;
  TrustedStatus PrepareTrustedValues(base::OnceClosure* callback) override;
  bool HandlesSetting(std::string_view path) const override;

  void SetTrustedStatus(TrustedStatus status);

  // Setters.
  void Set(std::string_view path, base::Value value);
  void Set(std::string_view path, bool value);
  void Set(std::string_view path, int value);
  void Set(std::string_view path, double value);
  void Set(std::string_view path, std::string_view value);

 private:
  PrefValueMap map_;
  TrustedStatus trusted_status_ = TRUSTED;
  std::vector<base::OnceClosure> callbacks_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SETTINGS_FAKE_CROS_SETTINGS_PROVIDER_H_
