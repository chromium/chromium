// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/android/hats/survey_client_android.h"
#include "chrome/browser/ui/android/hats/survey_ui_delegate_android.h"
#include "chrome/browser/ui/android/hats/test/test_survey_utils_bridge.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_wrapper.h"
#include "components/messages/android/test/messages_test_helper.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace hats {
namespace {
const char kTestSurveyTrigger[] = "testing";
const SurveyBitsData kTestSurveyProductSpecificBitsData{
    {"Test Field 1", true},
    {"Test Field 2", false}};
const SurveyStringData kTestSurveyProductSpecificStringData{
    {"Test Field 3", "Test value"}};

// Helper class that used to wait for message enqueued.
class MessageWaiter {
 public:
  explicit MessageWaiter(messages::MessagesTestHelper& messages_test_helper) {
    messages_test_helper.WaitForMessageEnqueued(
        base::BindOnce(&MessageWaiter::OnEvent, base::Unretained(this)));
  }
  ~MessageWaiter() = default;

  // Wait until the message dispatcher has a count change.
  [[nodiscard]] bool Wait() { return waiter_helper_.Wait(); }

 private:
  void OnEvent() { waiter_helper_.OnEvent(); }

  content::WaiterHelper waiter_helper_;
};
}  // namespace

class SurveyClientAndroidBrowserTest : public AndroidBrowserTest {
 public:
  SurveyClientAndroidBrowserTest() = default;
  ~SurveyClientAndroidBrowserTest() override = default;

  void SetUp() override {
    TestSurveyUtilsBridge::SetUpJavaTestSurveyFactory();
    AndroidBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Map all out-going DNS lookups to the local server.
    host_resolver()->AddRule("*", "127.0.0.1");

    messages_test_helper_.AttachTestMessageDispatcherForTesting(
        window_android());
  }

  void TearDown() override {
    messages_test_helper_.ResetMessageDispatcherForTesting();
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  ui::WindowAndroid* window_android() {
    return web_contents()->GetTopLevelNativeWindow();
  }

  messages::MessageWrapper* createMessage() {
    message_ = std::make_unique<messages::MessageWrapper>(
        messages::MessageIdentifier::TEST_MESSAGE,
        base::BindOnce(&SurveyClientAndroidBrowserTest::MessageAccepted,
                       base::Unretained(this)),
        base::BindOnce(&SurveyClientAndroidBrowserTest::MessageDeclined,
                       base::Unretained(this)));
    return message_.get();
  }

  bool message_accepted() { return message_accepted_; }

  bool message_declined() { return message_declined_; }

 protected:
  void MessageAccepted() { message_accepted_ = true; }

  void MessageDeclined(messages::DismissReason dismiss_reason) {
    message_declined_ = true;
  }

  messages::MessagesTestHelper messages_test_helper_;

 private:
  std::unique_ptr<messages::MessageWrapper> message_;
  bool message_accepted_;
  bool message_declined_;
};

IN_PROC_BROWSER_TEST_F(SurveyClientAndroidBrowserTest,
                       CreateSurveyWithMessage) {
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  auto* message = createMessage();
  std::unique_ptr<SurveyUiDelegateAndroid> delegate =
      std::make_unique<SurveyUiDelegateAndroid>(message, window_android());
  EXPECT_TRUE(delegate.get());

  // Create survey client with delegate.
  std::unique_ptr<SurveyClientAndroid> survey_client =
      std::make_unique<SurveyClientAndroid>(kTestSurveyTrigger, delegate.get());

  {
    MessageWaiter waiter(messages_test_helper_);
    survey_client->LaunchSurvey(window_android(),
                                kTestSurveyProductSpecificBitsData,
                                kTestSurveyProductSpecificStringData);
    EXPECT_TRUE(waiter.Wait());
  }

  EXPECT_EQ(1, messages_test_helper_.GetMessageCount(window_android()));
  EXPECT_EQ(static_cast<int>(messages::MessageIdentifier::TEST_MESSAGE),
            messages_test_helper_.GetMessageIdentifier(window_android(), 0));

  messages::MessageDispatcherBridge::Get()->DismissMessage(
      message, messages::DismissReason::UNKNOWN);
  EXPECT_TRUE(message_declined());
}

}  // namespace hats
