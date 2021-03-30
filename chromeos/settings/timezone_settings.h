// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SETTINGS_TIMEZONE_SETTINGS_H_
#define CHROMEOS_SETTINGS_TIMEZONE_SETTINGS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace chromeos {
namespace system {

// Canonical name of UTC timezone.
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kUTCTimezoneName[];

// This interface provides access to Chrome OS timezone settings.
class COMPONENT_EXPORT(CHROMEOS_SETTINGS) TimezoneSettings {
 public:
  class Observer {
   public:
    // Called when the timezone has changed. |timezone| is non-null.
    virtual void TimezoneChanged(const icu::TimeZone& timezone) = 0;
   protected:
    virtual ~Observer();
  };

  static TimezoneSettings* GetInstance();

  // Returns the current timezone as an icu::Timezone object.
  virtual const icu::TimeZone& GetTimezone() = 0;
  virtual std::u16string GetCurrentTimezoneID() = 0;

  // Sets the current timezone and notifies all Observers.
  virtual void SetTimezone(const icu::TimeZone& timezone) = 0;
  virtual void SetTimezoneFromID(const std::u16string& timezone_id) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual const std::vector<std::unique_ptr<icu::TimeZone>>& GetTimezoneList()
      const = 0;

  // Gets timezone ID which is also used as timezone pref value.
  static std::u16string GetTimezoneID(const icu::TimeZone& timezone);

 protected:
  virtual ~TimezoneSettings() {}
};

}  // namespace system
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace system {
using ::chromeos::system::TimezoneSettings;
}  // namespace system
}  // namespace ash

#endif  // CHROMEOS_SETTINGS_TIMEZONE_SETTINGS_H_
