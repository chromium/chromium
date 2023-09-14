// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_BROWSERTEST_H_

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "services/network/test/test_network_context.h"

class NetInternalsTest : public WebUIMochaBrowserTest {
 public:
  NetInternalsTest();

  NetInternalsTest(const NetInternalsTest&) = delete;
  NetInternalsTest& operator=(const NetInternalsTest&) = delete;

  ~NetInternalsTest() override;

  void SetUpOnMainThread() override;

 protected:
  void OnWebContentsAvailable(content::WebContents* web_contents) override;

 private:
  class MessageHandler;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_BROWSERTEST_H_
