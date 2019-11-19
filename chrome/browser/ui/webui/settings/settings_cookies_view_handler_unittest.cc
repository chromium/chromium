// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_cookies_view_handler.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_web_ui.h"

namespace settings {

class CookiesViewHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
    handler_ = std::make_unique<CookiesViewHandler>();
    handler_->set_web_ui(web_ui());
    handler_->AllowJavascript();
    web_ui_->ClearTrackedCalls();
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  CookiesViewHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<CookiesViewHandler> handler_;
};

// This unit test checks that the javascript callbacks are called correctly for
// the reloadCookies and the getDisplayList handler cases. It also makes sure
// that CHECKs for request_.callback_id_.empty() do not fire when multiple
// handlers are called in sequence.
TEST_F(CookiesViewHandlerTest, HandleReloadCookiesAndGetDisplayList) {
  const std::string reload_callback_id("localData.reload_0");
  const std::string get_display_list_callback_id("localData.getDisplayList_1");

  base::ListValue reload_args;
  reload_args.AppendString(reload_callback_id);
  handler()->HandleReloadCookies(&reload_args);
  EXPECT_EQ(1U, web_ui()->call_data().size());

  base::ListValue get_display_list_args;
  get_display_list_args.AppendString(reload_callback_id);
  get_display_list_args.AppendString(std::string());
  handler()->HandleGetDisplayList(&get_display_list_args);
  EXPECT_EQ(2U, web_ui()->call_data().size());
}

}  // namespace settings
