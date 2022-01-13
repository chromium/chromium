// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_CHROME_TIMETICKS_BROWSERTEST_H_
#define CHROME_TEST_DATA_WEBUI_CHROME_TIMETICKS_BROWSERTEST_H_

#include "base/memory/weak_ptr.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

// Test fixture for testing if chrome.timeTicks.nowInMicroseconds() is close
// to base::TimeTicks::Now().
class ChromeTimeTicksBrowserTest : public WebUIBrowserTest {
 public:
  ChromeTimeTicksBrowserTest();

  ChromeTimeTicksBrowserTest(const ChromeTimeTicksBrowserTest&) = delete;
  ChromeTimeTicksBrowserTest& operator=(const ChromeTimeTicksBrowserTest&) =
      delete;

  ~ChromeTimeTicksBrowserTest() override;

  class ChromeTimeTicksWebUIMessageHandler
      : public content::WebUIMessageHandler {
   public:
    ChromeTimeTicksWebUIMessageHandler();
    ~ChromeTimeTicksWebUIMessageHandler() override;

    void HandleCheckTimeTicks(base::Value::ConstListView args);

   private:
    void RegisterMessages() override;

    base::WeakPtrFactory<ChromeTimeTicksWebUIMessageHandler> weak_ptr_factory_{
        this};
  };

 protected:
  ChromeTimeTicksWebUIMessageHandler message_handler_;

 private:
  content::WebUIMessageHandler* GetMockMessageHandler() override;
};

#endif  // CHROME_TEST_DATA_WEBUI_CHROME_TIMETICKS_BROWSERTEST_H_
