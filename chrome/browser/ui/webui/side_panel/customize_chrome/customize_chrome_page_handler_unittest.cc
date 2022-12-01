// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

class CustomizeChromePageHandlerTest : public testing::Test {
 public:
  CustomizeChromePageHandlerTest()
      : handler_(mojo::PendingReceiver<
                     side_panel::mojom::CustomizeChromePageHandler>(),
                 &profile_) {}

  TestingProfile& profile() { return profile_; }
  CustomizeChromePageHandler& handler() { return handler_; }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  CustomizeChromePageHandler handler_;
};

TEST_F(CustomizeChromePageHandlerTest, SetMostVisitedSettings) {
  profile().GetPrefs()->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles, false);
  profile().GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);

  handler().SetMostVisitedSettings(/*custom_links_enabled=*/false,
                                   /*visible=*/true);

  EXPECT_TRUE(
      profile().GetPrefs()->GetBoolean(ntp_prefs::kNtpUseMostVisitedTiles));
  EXPECT_TRUE(
      profile().GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));
}

TEST_F(CustomizeChromePageHandlerTest, GetMostVisitedSettings) {
  profile().GetPrefs()->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles, false);
  profile().GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);

  base::MockCallback<CustomizeChromePageHandler::GetMostVisitedSettingsCallback>
      callback;
  bool custom_links_enabled = false;
  bool shortcuts_visible = false;
  EXPECT_CALL(callback, Run(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&custom_links_enabled, &shortcuts_visible](
              bool custom_links_enabled_arg, bool shortcuts_visible_arg) {
            custom_links_enabled = custom_links_enabled_arg;
            shortcuts_visible = shortcuts_visible_arg;
          }));
  handler().GetMostVisitedSettings(callback.Get());

  EXPECT_TRUE(custom_links_enabled);
  EXPECT_TRUE(shortcuts_visible);
}
