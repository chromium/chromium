// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/recently_audible_helper.h"

#include <list>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class RecentlyAudibleHelperTest : public testing::Test {
 public:
  RecentlyAudibleHelperTest() = default;

  RecentlyAudibleHelperTest(const RecentlyAudibleHelperTest&) = delete;
  RecentlyAudibleHelperTest& operator=(const RecentlyAudibleHelperTest&) =
      delete;

  ~RecentlyAudibleHelperTest() override {}

  void SetUp() override {
    test_web_contents_factory_ =
        std::make_unique<content::TestWebContentsFactory>();
    contents_ =
        test_web_contents_factory_->CreateWebContents(&testing_profile_);

    // Replace the main message loop with one that uses mock time.
    scoped_context_ =
        std::make_unique<base::TestMockTimeTaskRunner::ScopedContext>(
            task_runner_);

    RecentlyAudibleHelper::CreateForWebContents(contents_);
    helper_ = RecentlyAudibleHelper::FromWebContents(contents_);
    helper_->SetTickClockForTesting(task_runner_->GetMockTickClock());
    subscription_ = helper_->RegisterCallbackForTesting(base::BindRepeating(
        &RecentlyAudibleHelperTest::OnRecentlyAudibleCallback,
        base::Unretained(this)));
  }

  void TearDown() override {
    helper_->SetTickClockForTesting(nullptr);
    subscription_ = {};
    task_runner_->RunUntilIdle();
    EXPECT_TRUE(recently_audible_messages_.empty());

    scoped_context_.reset();
    test_web_contents_factory_.reset();
  }

  void SimulateAudioStarts() {
    content::WebContentsTester::For(contents_)->SetIsCurrentlyAudible(true);
  }

  void SimulateAudioStops() {
    content::WebContentsTester::For(contents_)->SetIsCurrentlyAudible(false);
  }

  void AdvanceTime(base::TimeDelta duration) {
    task_runner_->FastForwardBy(duration);
  }

  void ExpectNeverAudible() {
    EXPECT_FALSE(helper_->WasEverAudible());
    EXPECT_FALSE(helper_->IsCurrentlyAudible());
    EXPECT_FALSE(helper_->WasRecentlyAudible());
  }

  void ExpectCurrentlyAudible() {
    EXPECT_TRUE(helper_->WasEverAudible());
    EXPECT_TRUE(helper_->IsCurrentlyAudible());
    EXPECT_TRUE(helper_->WasRecentlyAudible());
  }

  void ExpectRecentlyAudible() {
    EXPECT_TRUE(helper_->WasEverAudible());
    EXPECT_FALSE(helper_->IsCurrentlyAudible());
    EXPECT_TRUE(helper_->WasRecentlyAudible());
  }

  void ExpectNotRecentlyAudible() {
    EXPECT_TRUE(helper_->WasEverAudible());
    EXPECT_FALSE(helper_->IsCurrentlyAudible());
    EXPECT_FALSE(helper_->WasRecentlyAudible());
  }

  void ExpectRecentlyAudibleTransition(bool recently_audible) {
    EXPECT_EQ(recently_audible, recently_audible_messages_.front());
    recently_audible_messages_.pop_front();
  }

  void VerifyAndClearExpectations() {
    EXPECT_TRUE(recently_audible_messages_.empty());
  }

 private:
  void OnRecentlyAudibleCallback(bool recently_audible) {
    recently_audible_messages_.push_back(recently_audible);
  }

  // Mock time environment.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  std::unique_ptr<base::TestMockTimeTaskRunner::ScopedContext> scoped_context_;

  // Environment for creating WebContents.
  std::unique_ptr<content::TestWebContentsFactory> test_web_contents_factory_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;

  // A test WebContents and its associated helper.
  raw_ptr<content::WebContents, DanglingUntriaged> contents_;
  raw_ptr<RecentlyAudibleHelper, DanglingUntriaged> helper_;
  base::CallbackListSubscription subscription_;

  std::list<bool> recently_audible_messages_;
};

TEST_F(RecentlyAudibleHelperTest, AllStateTransitions) {
  // Initially nothing has ever been audible.
  ExpectNeverAudible();
  VerifyAndClearExpectations();

  // Start audio and expect a transition.
  SimulateAudioStarts();
  ExpectRecentlyAudibleTransition(true);
  ExpectCurrentlyAudible();
  VerifyAndClearExpectations();

  // Keep audio playing and don't expect any transitions.
  AdvanceTime(base::Seconds(30));
  ExpectCurrentlyAudible();
  VerifyAndClearExpectations();

  // Stop audio, but don't expect a transition.
  SimulateAudioStops();
  ExpectRecentlyAudible();
  VerifyAndClearExpectations();

  // Advance time by half the timeout period. Still don't expect a transition.
  AdvanceTime(RecentlyAudibleHelper::kRecentlyAudibleTimeout / 2);
  ExpectRecentlyAudible();
  VerifyAndClearExpectations();

  // Start audio again. Still don't expect a transition.
  SimulateAudioStarts();
  ExpectCurrentlyAudible();
  VerifyAndClearExpectations();

  // Advance time and stop audio, not expecting a transition.
  AdvanceTime(base::Seconds(30));
  SimulateAudioStops();
  ExpectRecentlyAudible();
  VerifyAndClearExpectations();

  // Advance time by the timeout period and this time expect a transition to not
  // recently audible.
  AdvanceTime(RecentlyAudibleHelper::kRecentlyAudibleTimeout);
  ExpectRecentlyAudibleTransition(false);
  ExpectNotRecentlyAudible();
  VerifyAndClearExpectations();
}
