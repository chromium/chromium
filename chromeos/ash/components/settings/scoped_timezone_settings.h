// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SETTINGS_SCOPED_TIMEZONE_SETTINGS_H_
#define CHROMEOS_ASH_COMPONENTS_SETTINGS_SCOPED_TIMEZONE_SETTINGS_H_

#include <string>

#include "base/component_export.h"

namespace ash::system {

// Helper class to temporary change current timezone in tests and return it back
// to the original value on destruction.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SETTINGS)
    ScopedTimezoneSettings {
 public:
  ScopedTimezoneSettings();
  explicit ScopedTimezoneSettings(const std::u16string& new_timezone_id);
  ScopedTimezoneSettings(const ScopedTimezoneSettings&) = delete;
  ScopedTimezoneSettings& operator=(const ScopedTimezoneSettings&) = delete;
  ~ScopedTimezoneSettings();

  std::u16string GetCurrentTimezoneID() const;
  void SetTimezoneFromID(const std::u16string& new_timezone_id) const;

 private:
  void SaveOriginalTimezone();

  std::u16string original_timezone_id_;
};

}  // namespace ash::system

#endif  // CHROMEOS_ASH_COMPONENTS_SETTINGS_SCOPED_TIMEZONE_SETTINGS_H_
