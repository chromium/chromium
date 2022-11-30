// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_CHROME_SEND_BROWSERTEST_H_
#define CHROME_TEST_DATA_WEBUI_CHROME_SEND_BROWSERTEST_H_

#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

// Test fixture for testing chrome.send. This class registers the "checkSend"
// message, but should NOT receive it.
class ChromeSendWebUITest : public WebUIBrowserTest {
 public:
  ChromeSendWebUITest();

  ChromeSendWebUITest(const ChromeSendWebUITest&) = delete;
  ChromeSendWebUITest& operator=(const ChromeSendWebUITest&) = delete;

  ~ChromeSendWebUITest() override;

  // Mocked message handler class to register expects using gmock framework.
  class ChromeSendWebUIMessageHandler : public content::WebUIMessageHandler {
   public:
    ChromeSendWebUIMessageHandler();
    ~ChromeSendWebUIMessageHandler() override;

    MOCK_METHOD1(HandleCheckSend, void(const base::Value::List&));

   private:
    void RegisterMessages() override;
  };


 protected:
  // Strict mock will fail when unexpected chrome.send messages are received.
  ::testing::StrictMock<ChromeSendWebUIMessageHandler> message_handler_;

 private:
  content::WebUIMessageHandler* GetMockMessageHandler() override;
};

// Test fixture for verifying chrome.send messages are passed through. This
// class DOES expect to receive the "checkSend" message.
class ChromeSendPassthroughWebUITest : public ChromeSendWebUITest {
 public:
  ChromeSendPassthroughWebUITest();

  ChromeSendPassthroughWebUITest(const ChromeSendPassthroughWebUITest&) =
      delete;
  ChromeSendPassthroughWebUITest& operator=(
      const ChromeSendPassthroughWebUITest&) = delete;

  ~ChromeSendPassthroughWebUITest() override;

 private:
  void SetUpOnMainThread() override;
};

#endif  // CHROME_TEST_DATA_WEBUI_CHROME_SEND_BROWSERTEST_H_
