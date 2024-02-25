// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_FAKE_OS_SETTINGS_SECTIONS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_FAKE_OS_SETTINGS_SECTIONS_H_

#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_sections.h"

namespace ash::settings {

// Collection of FakeOsSettingsSections.
class FakeOsSettingsSections : public OsSettingsSections {
 public:
  FakeOsSettingsSections();
  FakeOsSettingsSections(const FakeOsSettingsSections& other) = delete;
  FakeOsSettingsSections& operator=(const FakeOsSettingsSections& other) =
      delete;
  ~FakeOsSettingsSections() override;

  void FillWithFakeSettings();
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_FAKE_OS_SETTINGS_SECTIONS_H_
