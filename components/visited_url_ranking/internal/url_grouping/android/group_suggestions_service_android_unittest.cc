// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/android/group_suggestions_service_android.h"

#include "base/android/jni_android.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_service_impl.h"
#include "components/visited_url_ranking/internal/url_grouping/tab_events_visit_transformer.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/visited_url_ranking/internal/test_jni_headers/TestServiceDelegate_jni.h"

namespace visited_url_ranking {

using ::base::android::AttachCurrentThread;
using ::base::android::ScopedJavaLocalRef;

// Java delegate for testing, counter part of TestServiceDelegate.
// On each observation increments a counter.
class TestJavaDelegate {
 public:
  TestJavaDelegate(GroupSuggestionsService* service, base::OnceClosure callback)
      : service_(GroupSuggestionsService::GetJavaObject(service)),
        java_obj_(Java_TestServiceDelegate_createAndAdd(
            AttachCurrentThread(),
            service_,
            reinterpret_cast<long>(this))),
        callback_(std::move(callback)) {}

  ~TestJavaDelegate() {
    Java_TestServiceDelegate_destroy(AttachCurrentThread(), java_obj_,
                                     service_);
  }

  int GetShowSuggestionCount() {
    return Java_TestServiceDelegate_getShowSuggestionCount(
        AttachCurrentThread(), java_obj_);
  }
  int GetOnDumpStateForFeedbackCount() {
    return Java_TestServiceDelegate_getOnDumpStateForFeedbackCount(
        AttachCurrentThread(), java_obj_);
  }

  void OnDelegateNotify() { std::move(callback_).Run(); }

 private:
  ScopedJavaLocalRef<jobject> service_;
  ScopedJavaLocalRef<jobject> java_obj_;

  base::OnceClosure callback_;
};

// Implements TestServiceObserver.onDelegateNotify static method.
static void JNI_TestServiceDelegate_OnDelegateNotify(JNIEnv* env,
                                                     jlong delegate_ptr) {
  reinterpret_cast<TestJavaDelegate*>(delegate_ptr)->OnDelegateNotify();
}

class GroupSuggestionsServiceAndroidTest : public testing::Test {
 public:
  GroupSuggestionsServiceAndroidTest() = default;

  ~GroupSuggestionsServiceAndroidTest() override = default;

  void SetUp() override {
    Test::SetUp();
    auto* registry = pref_service_.registry();
    GroupSuggestionsServiceImpl::RegisterProfilePrefs(registry);
    tab_events_visit_transformer_ =
        std::make_unique<TabEventsVisitTransformer>();
    mock_ranking_service_ = std::make_unique<MockVisitedURLRankingService>();
    group_suggestions_service_ = std::make_unique<GroupSuggestionsServiceImpl>(
        mock_ranking_service_.get(), tab_events_visit_transformer_.get(),
        &pref_service_);
    group_suggestions_service_android_ =
        std::make_unique<GroupSuggestionsServiceAndroid>(
            group_suggestions_service_.get());
  }

  void TearDown() override {
    group_suggestions_service_android_.reset();
    group_suggestions_service_.reset();
    mock_ranking_service_.reset();
    tab_events_visit_transformer_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TabEventsVisitTransformer> tab_events_visit_transformer_;
  std::unique_ptr<MockVisitedURLRankingService> mock_ranking_service_;
  std::unique_ptr<GroupSuggestionsServiceImpl> group_suggestions_service_;
  std::unique_ptr<GroupSuggestionsServiceAndroid>
      group_suggestions_service_android_;
};

TEST_F(GroupSuggestionsServiceAndroidTest, ShowSuggestionNotifyDelegate) {
  base::RunLoop run_loop;
  TestJavaDelegate delegate(group_suggestions_service_.get(),
                            run_loop.QuitClosure());

  EXPECT_EQ(delegate.GetShowSuggestionCount(), 0);

  group_suggestions_service_->GetTabEventTracker()->DidAddTab(1, 0);

  // TODO(yuezhanggg): Updateh the test case when the ShowSuggestion event is
  // populated.
  EXPECT_EQ(delegate.GetShowSuggestionCount(), 0);
}

}  // namespace visited_url_ranking

DEFINE_JNI(TestServiceDelegate)
