// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"

#include "base/values.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_web_ui.h"

class CookieControlsHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_ui_.set_web_contents(web_contents());
    handler_ = std::make_unique<CookieControlsHandler>();
    handler_->set_web_ui(&web_ui_);
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  content::TestWebUI web_ui_;
  std::unique_ptr<CookieControlsHandler> handler_;
};

TEST_F(CookieControlsHandlerTest, HandleCookieControlsToggleChanged) {
  EXPECT_EQ(
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly),
      Profile::FromWebUI(&web_ui_)->GetPrefs()->GetInteger(
          prefs::kCookieControlsMode));
  base::ListValue args_false;
  args_false.AppendBoolean(false);
  handler_->HandleCookieControlsToggleChanged(&args_false);
  EXPECT_EQ(static_cast<int>(content_settings::CookieControlsMode::kOff),
            Profile::FromWebUI(&web_ui_)->GetPrefs()->GetInteger(
                prefs::kCookieControlsMode));
  base::ListValue args_true;
  args_true.AppendBoolean(true);
  handler_->HandleCookieControlsToggleChanged(&args_true);
  EXPECT_EQ(
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly),
      Profile::FromWebUI(&web_ui_)->GetPrefs()->GetInteger(
          prefs::kCookieControlsMode));
}
