// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_
#define CHROMEOS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
class Value;
}

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_SETTINGS) SystemSettingsProvider
    : public CrosSettingsProvider,
      public system::TimezoneSettings::Observer {
 public:
  SystemSettingsProvider();
  explicit SystemSettingsProvider(const NotifyObserversCallback& notify_cb);
  ~SystemSettingsProvider() override;

  // CrosSettingsProvider implementation.
  const base::Value* Get(const std::string& path) const override;
  TrustedStatus PrepareTrustedValues(const base::Closure& callback) override;
  bool HandlesSetting(const std::string& path) const override;

  // TimezoneSettings::Observer implementation.
  void TimezoneChanged(const icu::TimeZone& timezone) override;

 private:
  // Code common to both constructors.
  void Init();

  std::unique_ptr<base::Value> timezone_value_;
  std::unique_ptr<base::Value> per_user_timezone_enabled_value_;
  std::unique_ptr<base::Value> fine_grained_time_zone_enabled_value_;

  DISALLOW_COPY_AND_ASSIGN(SystemSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_
