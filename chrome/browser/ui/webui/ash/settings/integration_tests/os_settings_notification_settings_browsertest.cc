// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_lock_screen_browser_test_base.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom-test-utils.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

// Tests the toggle that controls notifications on the lock screen in the
// lock-screen section of the chrome://os-settings webui.
class OSSettingsNotificationSettingsTest
    : public OSSettingsLockScreenBrowserTestBase {
 public:
  OSSettingsNotificationSettingsTest()
      : OSSettingsLockScreenBrowserTestBase(ash::AshAuthFactor::kGaiaPassword) {
    feature_list_.InitAndEnableFeature(ash::features::kLockScreenNotifications);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the deep link to the notification toggle works.
IN_PROC_BROWSER_TEST_F(OSSettingsNotificationSettingsTest,
                       NotificationSettings) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsDeepLinkAndAuthenticate(
          base::NumberToString(static_cast<int>(
              chromeos::settings::mojom::Setting::kLockScreenNotification)));
  lock_screen_settings.AssertLockScreenNotificationFocused();
}

}  // namespace ash::settings
