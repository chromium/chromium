// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_BROWSERTEST_H_

#include <memory>

#include "base/macros.h"
#include "chrome/test/base/web_ui_browser_test.h"

class NetInternalsTest : public WebUIBrowserTest {
 protected:
  NetInternalsTest();
  ~NetInternalsTest() override;

  void SetUpOnMainThread() override;

 private:
  class MessageHandler;

  // WebUIBrowserTest implementation.
  content::WebUIMessageHandler* GetMockMessageHandler() override;

  // Attempts to start the test server.  Returns true on success or if the
  // TestServer is already started.
  bool StartTestServer();

  std::unique_ptr<MessageHandler> message_handler_;

  // True if the test server has already been successfully started.
  bool test_server_started_;

  DISALLOW_COPY_AND_ASSIGN(NetInternalsTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_BROWSERTEST_H_
