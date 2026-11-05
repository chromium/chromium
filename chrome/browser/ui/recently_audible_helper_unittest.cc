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
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
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

  ~RecentlyAudibleHelperTest() override = default;

  void SetUp() override {
    test_web_contents_factory_ =
        std::make_unique<content::TestWebContentsFactory>();
    contents_ =
        test_web_contents_factory_->CreateWebContents(&testing_profile_);

    // Replace the main message loop with one that uses mock time.
    scoped_context_ =
        std::make_unique<base::TestMockTimeTaskRunner::ScopedContext>(
            task_runner_);

    tabs::TabLookupFromWebContents::CreateForWebContents(contents_, &tab_);
    owned_helper_ = std::make_unique<RecentlyAudibleHelper>(tab_, contents_);
    helper_ = owned_helper_.get();
    helper_->SetTickClockForTesting(task_runner_->GetMockTickClock());
    subscription_ =
        helper_->RegisterRecentlyAudibleChangedCallback(base::BindRepeating(
            &RecentlyAudibleHelperTest::OnRecentlyAudibleCallback,
            base::Unretained(this)));
  }

  void TearDown() override {
    if (helper_) {
      helper_->SetTickClockForTesting(nullptr);
    }
    subscription_ = {};
    task_runner_->RunUntilIdle();
    EXPECT_TRUE(recently_audible_messages_.empty());

    helper_ = nullptr;
    contents_ = nullptr;
    owned_helper_.reset();
    scoped_context_.reset();
    test_web_contents_factory_.reset();
  }

  void InitForNonTabWebContents(bool initially_audible = false) {
    if (helper_) {
      helper_->SetTickClockForTesting(nullptr);
    }
    subscription_ = {};
    helper_ = nullptr;
    owned_helper_.reset();

    contents_ =
        test_web_contents_factory_->CreateWebContents(&testing_profile_);
    if (initially_audible) {
      SimulateAudioStarts();
    }
    RecentlyAudibleHelper::CreateForWebContents(contents_);
    helper_ = RecentlyAudibleHelper::FromWebContents(contents_);
    ASSERT_NE(nullptr, helper_);
    helper_->SetTickClockForTesting(task_runner_->GetMockTickClock());
    subscription_ =
        helper_->RegisterRecentlyAudibleChangedCallback(base::BindRepeating(
            &RecentlyAudibleHelperTest::OnRecentlyAudibleCallback,
            base::Unretained(this)));
  }

  content::WebContents* CreateWebContents() {
    return test_web_contents_factory_->CreateWebContents(&testing_profile_);
  }

  content::WebContents* contents() { return contents_; }
  tabs::MockTabInterface& tab() { return tab_; }
  RecentlyAudibleHelper* helper() { return helper_; }

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
  tabs::MockTabInterface tab_;
  std::unique_ptr<RecentlyAudibleHelper> owned_helper_;
  raw_ptr<RecentlyAudibleHelper> helper_;
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

TEST_F(RecentlyAudibleHelperTest, FromAndFromWebContents) {
  // Null lookups return nullptr.
  EXPECT_EQ(nullptr, RecentlyAudibleHelper::From(nullptr));
  EXPECT_EQ(nullptr, RecentlyAudibleHelper::FromWebContents(nullptr));

  // Tab-attached helper is retrievable via both From(tab) and
  // FromWebContents(contents).
  EXPECT_EQ(&tab(), tabs::TabInterface::MaybeGetFromContents(contents()));
  EXPECT_EQ(helper(), RecentlyAudibleHelper::From(&tab()));
  EXPECT_EQ(helper(), RecentlyAudibleHelper::FromWebContents(contents()));

  // Calling CreateForWebContents / GetOrCreateForWebContents on a tab-backed
  // WebContents is a no-op that returns the tab-attached helper without
  // creating a duplicate WebContentsUserData instance.
  RecentlyAudibleHelper::CreateForWebContents(contents());
  EXPECT_EQ(helper(), RecentlyAudibleHelper::FromWebContents(contents()));
  EXPECT_EQ(helper(),
            RecentlyAudibleHelper::GetOrCreateForWebContents(contents()));
  EXPECT_EQ(
      nullptr,
      content::WebContentsUserData<RecentlyAudibleHelper>::FromWebContents(
          contents()));

  // A non-tabbed WebContents without a helper attached returns nullptr.
  content::WebContents* non_tab_contents = CreateWebContents();
  EXPECT_EQ(nullptr,
            tabs::TabInterface::MaybeGetFromContents(non_tab_contents));
  EXPECT_EQ(nullptr, RecentlyAudibleHelper::FromWebContents(non_tab_contents));

  // Attaching via CreateForWebContents makes it retrievable via
  // FromWebContents(non_tab_contents), and subsequent CreateForWebContents /
  // GetOrCreateForWebContents calls return the same instance.
  RecentlyAudibleHelper::CreateForWebContents(non_tab_contents);
  EXPECT_EQ(nullptr,
            tabs::TabInterface::MaybeGetFromContents(non_tab_contents));
  RecentlyAudibleHelper* non_tab_helper =
      RecentlyAudibleHelper::FromWebContents(non_tab_contents);
  EXPECT_NE(nullptr, non_tab_helper);
  EXPECT_NE(helper(), non_tab_helper);
  RecentlyAudibleHelper::CreateForWebContents(non_tab_contents);
  EXPECT_EQ(non_tab_helper,
            RecentlyAudibleHelper::FromWebContents(non_tab_contents));
  EXPECT_EQ(non_tab_helper,
            RecentlyAudibleHelper::GetOrCreateForWebContents(non_tab_contents));
}

TEST_F(RecentlyAudibleHelperTest, CreateForWebContentsAllStateTransitions) {
  ASSERT_NO_FATAL_FAILURE(InitForNonTabWebContents());
  EXPECT_EQ(nullptr, tabs::TabInterface::MaybeGetFromContents(contents()));
  EXPECT_EQ(helper(), RecentlyAudibleHelper::FromWebContents(contents()));

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

TEST_F(RecentlyAudibleHelperTest, CreateForWebContentsInitiallyAudible) {
  ASSERT_NO_FATAL_FAILURE(InitForNonTabWebContents(/*initially_audible=*/true));
  EXPECT_EQ(nullptr, tabs::TabInterface::MaybeGetFromContents(contents()));
  EXPECT_EQ(helper(), RecentlyAudibleHelper::FromWebContents(contents()));

  // Helper created on an already-audible WebContents starts in the currently
  // audible state without emitting a transition callback.
  ExpectCurrentlyAudible();
  VerifyAndClearExpectations();

  // Advance time so the mock tick clock is non-null when audio stops, then stop
  // audio; helper remains recently audible until the timeout elapses.
  AdvanceTime(base::Seconds(30));
  SimulateAudioStops();
  ExpectRecentlyAudible();
  VerifyAndClearExpectations();

  // Advance time by the timeout period and expect a transition to not recently
  // audible.
  AdvanceTime(RecentlyAudibleHelper::kRecentlyAudibleTimeout);
  ExpectRecentlyAudibleTransition(false);
  ExpectNotRecentlyAudible();
  VerifyAndClearExpectations();
}
