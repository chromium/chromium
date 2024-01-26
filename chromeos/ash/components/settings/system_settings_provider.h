// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_

#include <memory>
#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SETTINGS) SystemSettingsProvider
    : public CrosSettingsProvider,
      public system::TimezoneSettings::Observer {
 public:
  SystemSettingsProvider();
  explicit SystemSettingsProvider(const NotifyObserversCallback& notify_cb);

  SystemSettingsProvider(const SystemSettingsProvider&) = delete;
  SystemSettingsProvider& operator=(const SystemSettingsProvider&) = delete;

  ~SystemSettingsProvider() override;

  // CrosSettingsProvider implementation.
  const base::Value* Get(std::string_view path) const override;
  TrustedStatus PrepareTrustedValues(base::OnceClosure* callback) override;
  bool HandlesSetting(std::string_view path) const override;

  // TimezoneSettings::Observer implementation.
  void TimezoneChanged(const icu::TimeZone& timezone) override;

 private:
  // Code common to both constructors.
  void Init();

  std::unique_ptr<base::Value> timezone_value_;
  std::unique_ptr<base::Value> per_user_timezone_enabled_value_;
  std::unique_ptr<base::Value> fine_grained_time_zone_enabled_value_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_
