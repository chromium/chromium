// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client_impl.h"

#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "url/gurl.h"

namespace {

class TestSettingsWindowManager : public chrome::SettingsWindowManager {
 public:
  void ShowChromePageForProfile(Profile* profile,
                                const GURL& gurl,
                                int64_t display_id) override {
    last_url_ = gurl;
  }

  const GURL& last_url() { return last_url_; }

 private:
  GURL last_url_;
};

// Use BrowserWithTestWindowTest because it sets up ash::Shell, ash::SystemTray,
// ProfileManager, etc.
using SystemTrayClientImplTest = BrowserWithTestWindowTest;

TEST_F(SystemTrayClientImplTest, ShowAccountSettings) {
  SystemTrayClientImpl client_impl;

  TestSettingsWindowManager test_manager;
  chrome::SettingsWindowManager::SetInstanceForTesting(&test_manager);

  client_impl.ShowAccountSettings();
  EXPECT_EQ(
      test_manager.last_url(),
      chrome::GetOSSettingsUrl(chromeos::settings::mojom::kPeopleSectionPath));

  chrome::SettingsWindowManager::SetInstanceForTesting(nullptr);
}

}  // namespace
