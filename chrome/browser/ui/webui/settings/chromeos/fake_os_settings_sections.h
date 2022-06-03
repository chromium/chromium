// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FAKE_OS_SETTINGS_SECTIONS_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FAKE_OS_SETTINGS_SECTIONS_H_

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_sections.h"

namespace chromeos {
namespace settings {

// Collection of FakeOsSettingsSections.
class FakeOsSettingsSections : public OsSettingsSections {
 public:
  FakeOsSettingsSections();
  FakeOsSettingsSections(const FakeOsSettingsSections& other) = delete;
  FakeOsSettingsSections& operator=(const FakeOsSettingsSections& other) =
      delete;
  ~FakeOsSettingsSections() override;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FAKE_OS_SETTINGS_SECTIONS_H_
