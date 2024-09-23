// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/hats/hats_service_android.h"
#include "chrome/browser/ui/android/hats/survey_client_android.h"
#include "chrome/browser/ui/android/hats/test/test_survey_utils_bridge.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/test/messages_test_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

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

class SurveyObserver {
 public:
  SurveyObserver() = default;
  void Accept() { accepted_ = true; }

  void Dismiss() { dismissed_ = true; }

  bool IsAccepted() { return accepted_; }

  bool IsDismissed() { return dismissed_; }

  base::WeakPtr<SurveyObserver> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool accepted_ = false;
  bool dismissed_ = false;

  base::WeakPtrFactory<SurveyObserver> weak_ptr_factory_{this};
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

  HatsServiceAndroid* GetHatsService() {
    HatsServiceAndroid* service =
        static_cast<HatsServiceAndroid*>(HatsServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
            true));
    return service;
  }

 protected:
  messages::MessagesTestHelper messages_test_helper_;
};

IN_PROC_BROWSER_TEST_F(SurveyClientAndroidBrowserTest, LaunchSurvey) {
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  SurveyObserver observer;

  auto* hatsService = GetHatsService();
  {
    MessageWaiter waiter(messages_test_helper_);
    hatsService->LaunchSurveyForWebContents(
        kTestSurveyTrigger, web_contents(), kTestSurveyProductSpecificBitsData,
        kTestSurveyProductSpecificStringData,
        base::BindOnce(&SurveyObserver::Accept, observer.GetWeakPtr()),
        base::BindOnce(&SurveyObserver::Dismiss, observer.GetWeakPtr()));
    EXPECT_TRUE(waiter.Wait());
  }

  hatsService->GetFirstTaskForTesting()
      .GetMessageForTesting()
      ->HandleActionClick(base::android::AttachCurrentThread());

  EXPECT_TRUE(observer.IsAccepted());
}

}  // namespace hats
