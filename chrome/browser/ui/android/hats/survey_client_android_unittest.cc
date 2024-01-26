// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/survey_client_android.h"

#include <memory>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/hats/survey_ui_delegate_android.h"
#include "chrome/browser/ui/android/hats/test/test_survey_utils_bridge.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

using base::android::ScopedJavaGlobalRef;

namespace hats {
namespace {
const char kTestSurveyTrigger[] = "testing";
const SurveyBitsData kTestSurveyProductSpecificBitsData{
    {"Test Field 1", true},
    {"Test Field 2", false}};
const SurveyStringData kTestSurveyProductSpecificStringData{
    {"Test Field 3", "Test value"}};
}  // namespace

// Custom implementation resenting a C++ implemented SurveyUiDelegate.
class TestSurveyUiDelegate : public SurveyUiDelegateAndroid {
 public:
  explicit TestSurveyUiDelegate(JNIEnv* env) : env_(env) {}

  ~TestSurveyUiDelegate() override = default;

  void AcceptSurvey() {
    DCHECK(on_accepted_callback_);
    base::android::RunRunnableAndroid(on_accepted_callback_);
  }

  void DeclineSurvey() {
    DCHECK(on_declined_callback_);
    base::android::RunRunnableAndroid(on_declined_callback_);
  }

  void ShowSurveyInvitation(
      JNIEnv* env,
      const JavaParamRef<jobject>& on_accepted_callback,
      const JavaParamRef<jobject>& on_declined_callback,
      const JavaParamRef<jobject>& on_presentation_failed_callback) override {
    on_accepted_callback_ = on_accepted_callback;
    on_declined_callback_ = on_declined_callback;
    on_presentation_failed_callback_ = on_presentation_failed_callback;
  }

  // Dismiss the survey invitation.
  void Dismiss(JNIEnv* env) override {}

 private:
  raw_ptr<JNIEnv> env_;
  ScopedJavaGlobalRef<jobject> on_accepted_callback_;
  ScopedJavaGlobalRef<jobject> on_declined_callback_;
  ScopedJavaGlobalRef<jobject> on_presentation_failed_callback_;
};

class SurveyClientAndroidTest : public testing::Test {
 public:
  SurveyClientAndroidTest(const SurveyClientAndroidTest&) = delete;
  SurveyClientAndroidTest& operator=(const SurveyClientAndroidTest&) = delete;

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  SurveyClientAndroidTest() = default;

  void SetUp() override {
    testing::Test::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test_profile");
    TestSurveyUtilsBridge::SetUpJavaTestSurveyFactory();
  }

  std::unique_ptr<SurveyClientAndroid> survey_client_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

TEST_F(SurveyClientAndroidTest, CreateSurveyClientWithStaticTriggerId) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  JNIEnv* env = base::android::AttachCurrentThread();
  std::unique_ptr<TestSurveyUiDelegate> delegate =
      std::make_unique<TestSurveyUiDelegate>(env);
  survey_client_ = std::make_unique<SurveyClientAndroid>(
      kTestSurveyTrigger, delegate.get(), profile_,
      /*supplied_trigger_id=*/std::nullopt);

  survey_client_->LaunchSurvey(window->get(),
                               kTestSurveyProductSpecificBitsData,
                               kTestSurveyProductSpecificStringData);

  delegate->AcceptSurvey();

  std::string last_shown_trigger =
      TestSurveyUtilsBridge::GetLastShownSurveyTriggerId();
  ASSERT_FALSE(last_shown_trigger.empty());
  ASSERT_EQ(last_shown_trigger, kHatsNextSurveyTriggerIDTesting);
}

TEST_F(SurveyClientAndroidTest, CreateSurveyClientWithDynamicTriggerId) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  JNIEnv* env = base::android::AttachCurrentThread();
  std::unique_ptr<TestSurveyUiDelegate> delegate =
      std::make_unique<TestSurveyUiDelegate>(env);

  const std::string kSuppliedTriggerId = "SomeOtherId";
  survey_client_ = std::make_unique<SurveyClientAndroid>(
      kTestSurveyTrigger, delegate.get(), profile_,
      /*supplied_trigger_id=*/kSuppliedTriggerId);

  survey_client_->LaunchSurvey(window->get(),
                               kTestSurveyProductSpecificBitsData,
                               kTestSurveyProductSpecificStringData);

  delegate->AcceptSurvey();

  std::string last_shown_trigger =
      TestSurveyUtilsBridge::GetLastShownSurveyTriggerId();
  ASSERT_FALSE(last_shown_trigger.empty());
  ASSERT_EQ(last_shown_trigger, kSuppliedTriggerId);
}

}  // namespace hats
