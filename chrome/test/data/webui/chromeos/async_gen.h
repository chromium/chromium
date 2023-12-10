// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_TEST_DATA_WEBUI_CHROMEOS_ASYNC_GEN_H_
#define CHROME_TEST_DATA_WEBUI_CHROMEOS_ASYNC_GEN_H_

#include "chrome/test/base/ash/web_ui_browser_test.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

// C++ support class for javascript-generated asynchronous tests.
class WebUIBrowserAsyncGenTest : public WebUIBrowserTest {
 public:
  WebUIBrowserAsyncGenTest();

  WebUIBrowserAsyncGenTest(const WebUIBrowserAsyncGenTest&) = delete;
  WebUIBrowserAsyncGenTest& operator=(const WebUIBrowserAsyncGenTest&) = delete;

  ~WebUIBrowserAsyncGenTest() override;

 protected:
  class AsyncWebUIMessageHandler : public content::WebUIMessageHandler {
   public:
    AsyncWebUIMessageHandler();
    ~AsyncWebUIMessageHandler() override;

    MOCK_METHOD1(HandleTearDown, void(const base::Value::List&));

   private:
    void HandleCallJS(const base::Value::List& list_value);

    // WebUIMessageHandler implementation.
    void RegisterMessages() override;
  };

  // Handler for this test fixture.
  ::testing::StrictMock<AsyncWebUIMessageHandler> message_handler_;

 private:
  // Provide this object's handler.
  content::WebUIMessageHandler* GetMockMessageHandler() override {
    return &message_handler_;
  }

  void SetUpOnMainThread() override {
    WebUIBrowserTest::SetUpOnMainThread();
    EXPECT_CALL(message_handler_, HandleTearDown(::testing::_));
  }
};

#endif  // CHROME_TEST_DATA_WEBUI_CHROMEOS_ASYNC_GEN_H_
