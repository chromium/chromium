// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/permission_chip.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/permission_quiet_chip.h"
#include "chrome/browser/ui/views/location_bar/permission_request_chip.h"
#include "components/permissions/test/mock_permission_request.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"

namespace {

class TestDelegate : public permissions::PermissionPrompt::Delegate {
 public:
  explicit TestDelegate(
      const GURL& origin,
      const std::vector<permissions::RequestType> request_types) {
    std::transform(
        request_types.begin(), request_types.end(),
        std::back_inserter(requests_), [&](auto& request_type) {
          return std::make_unique<permissions::MockPermissionRequest>(
              origin, request_type);
        });
    std::transform(requests_.begin(), requests_.end(),
                   std::back_inserter(raw_requests_),
                   [](auto& req) { return req.get(); });
  }

  const std::vector<permissions::PermissionRequest*>& Requests() override {
    return raw_requests_;
  }

  GURL GetRequestingOrigin() const override {
    return raw_requests_.front()->requesting_origin();
  }

  GURL GetEmbeddingOrigin() const override {
    return GURL("https://embedder.example.com");
  }

  void Accept() override { requests_.clear(); }
  void AcceptThisTime() override { requests_.clear(); }
  void Deny() override { requests_.clear(); }
  void Dismiss() override { requests_.clear(); }
  void Ignore() override { requests_.clear(); }
  void SetManageClicked() override { requests_.clear(); }
  void SetLearnMoreClicked() override { requests_.clear(); }

  bool WasCurrentRequestAlreadyDisplayed() override {
    return was_current_request_already_displayed_;
  }
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override {
    return false;
  }
  bool ShouldCurrentRequestUseQuietUI() const override { return false; }
  absl::optional<permissions::PermissionUiSelector::QuietUiReason>
  ReasonForUsingQuietUi() const override {
    return absl::nullopt;
  }
  void SetDismissOnTabClose() override {}
  void SetBubbleShown() override {}
  void SetDecisionTime() override {}

  base::WeakPtr<permissions::PermissionPrompt::Delegate> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  bool IsRequestInProgress() { return !requests_.empty(); }

  void SetAlreadyDisplayed() { was_current_request_already_displayed_ = true; }

 private:
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests_;
  std::vector<permissions::PermissionRequest*> raw_requests_;
  bool was_current_request_already_displayed_ = false;
  base::WeakPtrFactory<TestDelegate> weak_factory_{this};
};
}  // namespace

class PermissionChipUnitTest : public TestWithBrowserView {
 public:
  PermissionChipUnitTest()
      : TestWithBrowserView(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  PermissionChipUnitTest(const PermissionChipUnitTest&) = delete;
  PermissionChipUnitTest& operator=(const PermissionChipUnitTest&) = delete;

  void SetUp() override {
    TestWithBrowserView::SetUp();

    AddTab(browser(), GURL("http://a.com"));
    web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void ClickOnChip(PermissionChip& chip) {
    views::test::ButtonTestApi(chip.button())
        .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0));
    base::RunLoop().RunUntilIdle();
  }

  raw_ptr<content::WebContents> web_contents_;

  base::TimeDelta kChipCollapseDuration = base::Seconds(12);
  base::TimeDelta kNormalChipDismissDuration = base::Seconds(6);
  base::TimeDelta kQuietChipDismissDuration = base::Seconds(18);
  base::TimeDelta kLongerThanAllTimersDuration = base::Seconds(50);
};

TEST_F(PermissionChipUnitTest, DisplayChipNoAutoPopupTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications});
  PermissionChip chip;
  chip.SetupChip(
      std::make_unique<PermissionRequestChip>(browser(), &delegate, false));

  EXPECT_FALSE(chip.IsBubbleShowing());

  // Due to animation issue, the collapse timer will not be started.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // Animation does not work. Most probably it is unit tests limitations.
  // `chip.is_fully_collapsed()` will not work as well.
  EXPECT_TRUE(chip.get_chip_button_for_testing()->is_animating());

  // TODO(crbug.com/1271093): Fix animation callback for unit tests.
  chip.stop_animation_for_test();
  EXPECT_FALSE(chip.get_chip_button_for_testing()->is_animating());

  EXPECT_TRUE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // The chip collapse timer is 12 seconds. After 11 seconds the permission
  // request should still be there.
  task_environment()->AdvanceClock(kChipCollapseDuration - base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_TRUE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // The collapse timer has 1 more second to go. Wait 2 seconds for the dismiss
  // timer to start.
  task_environment()->AdvanceClock(base::Seconds(2));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // The collapse timer is fired and the dismiss timer is started.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, AlreadyDisplayedRequestTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications});
  delegate.SetAlreadyDisplayed();

  EXPECT_TRUE(delegate.WasCurrentRequestAlreadyDisplayed());

  PermissionChip chip;
  chip.SetupChip(
      std::make_unique<PermissionRequestChip>(browser(), &delegate, false));

  EXPECT_FALSE(chip.IsBubbleShowing());

  // The permission request was already displayed, hence the dismiss timer will
  // be triggered directly after the chip is displayed.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  // The default dismiss timer is 6 seconds. The chip should be still displayed
  // after 5 seconds.
  task_environment()->AdvanceClock(kNormalChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // Wait 2 more seconds for the dismiss timer to finish.
  task_environment()->AdvanceClock(base::Seconds(2));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, MultiClickOnChipNoAutoPopupTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications});
  PermissionChip chip;
  chip.SetupChip(
      std::make_unique<PermissionRequestChip>(browser(), &delegate, false));

  EXPECT_FALSE(chip.IsBubbleShowing());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // Animation does not work. Most probably it is unit tests limitations.
  // `chip.is_fully_collapsed()` will not work as well.
  EXPECT_TRUE(chip.get_chip_button_for_testing()->is_animating());
  chip.stop_animation_for_test();
  EXPECT_FALSE(chip.get_chip_button_for_testing()->is_animating());

  EXPECT_TRUE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kChipCollapseDuration - base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_TRUE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  ClickOnChip(chip);
  EXPECT_TRUE(chip.IsBubbleShowing());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // After a very long time the permissin prompt popup bubble should still be
  // visible.
  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());
  EXPECT_TRUE(chip.IsBubbleShowing());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // The seconds click on the chip hides the popup bubble.
  ClickOnChip(chip);
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // After the second click, only dismiss timer should be active.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // The third click on the chip opens the popup bubble again.
  ClickOnChip(chip);
  EXPECT_TRUE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  ClickOnChip(chip);
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, DisplayChipAutoPopupTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications});
  PermissionChip chip;
  chip.SetupChip(
      std::make_unique<PermissionRequestChip>(browser(), &delegate, true));

  // Due to animation issue, the collapse timer will not be started.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // Animation does not work. Most probably it is unit tests limitations.
  // `chip.is_fully_collapsed()` will not work as well.
  EXPECT_TRUE(chip.get_chip_button_for_testing()->is_animating());
  chip.stop_animation_for_test();
  EXPECT_FALSE(chip.get_chip_button_for_testing()->is_animating());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());
  // Bubble is showing automatically.
  EXPECT_TRUE(chip.IsBubbleShowing());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // The seconds click on the chip hides the popup bubble.
  ClickOnChip(chip);
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // After the second click, only dismiss timer should be active.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, MultiClickOnChipAutoPopupTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications});
  PermissionChip chip;
  chip.SetupChip(
      std::make_unique<PermissionRequestChip>(browser(), &delegate, true));

  EXPECT_FALSE(chip.IsBubbleShowing());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // Animation does not work. Most probably it is unit tests limitations.
  // `chip.is_fully_collapsed()` will not work as well.
  EXPECT_TRUE(chip.get_chip_button_for_testing()->is_animating());
  chip.stop_animation_for_test();
  EXPECT_FALSE(chip.get_chip_button_for_testing()->is_animating());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // The permission prompt bubble is open automatically
  EXPECT_TRUE(chip.IsBubbleShowing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());
  EXPECT_TRUE(chip.IsBubbleShowing());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  ClickOnChip(chip);

  // The permission prompt bubble is open automatically, hence the first click
  // on the chip should close the bubble.
  EXPECT_FALSE(chip.IsBubbleShowing());
  EXPECT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  ClickOnChip(chip);
  EXPECT_TRUE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  ClickOnChip(chip);
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, DisplayQuietChipNoAbusiveTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications});
  PermissionChip chip;
  chip.SetupChip(
      std::make_unique<PermissionQuietChip>(browser(), &delegate, true));

  EXPECT_FALSE(chip.IsBubbleShowing());

  // Due to animation issue, the collapse timer will not be started.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // Animation does not work. Most probably it is unit tests limitations.
  // `chip.is_fully_collapsed()` will not work as well.
  EXPECT_TRUE(chip.get_chip_button_for_testing()->is_animating());
  chip.stop_animation_for_test();
  EXPECT_FALSE(chip.get_chip_button_for_testing()->is_animating());

  EXPECT_TRUE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kChipCollapseDuration - base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_TRUE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // Wait 2 more seconds for the collapse timer to finish.
  task_environment()->AdvanceClock(base::Seconds(2));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // The collapse timer is fired and the dismiss timer is started.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, MultiClickOnQuietChipNoAbusiveTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications});
  PermissionChip chip;
  chip.SetupChip(
      std::make_unique<PermissionQuietChip>(browser(), &delegate, true));

  EXPECT_FALSE(chip.IsBubbleShowing());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // Animation does not work. Most probably it is unit tests limitations.
  // `chip.is_fully_collapsed()` will not work as well.
  EXPECT_TRUE(chip.get_chip_button_for_testing()->is_animating());
  chip.stop_animation_for_test();
  EXPECT_FALSE(chip.get_chip_button_for_testing()->is_animating());

  EXPECT_TRUE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());
  EXPECT_FALSE(chip.IsBubbleShowing());

  // The chip collapse timer is 12 seconds.
  task_environment()->AdvanceClock(kChipCollapseDuration - base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_TRUE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  ClickOnChip(chip);
  EXPECT_TRUE(chip.IsBubbleShowing());

  // Collapse timer was restarted.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // After 30 seconds the permissin prompt popup bubble should still be visible.
  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());
  EXPECT_TRUE(chip.IsBubbleShowing());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // The seconds click on the chip hides the popup bubble.
  ClickOnChip(chip);
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // After the second click, only dismiss timer should be active.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // The third click on the chip opens the popup bubble again.
  ClickOnChip(chip);
  EXPECT_TRUE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  ClickOnChip(chip);
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, DisplayQuietChipAbusiveTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications});
  PermissionChip chip;
  chip.SetupChip(
      std::make_unique<PermissionQuietChip>(browser(), &delegate, false));

  EXPECT_FALSE(chip.IsBubbleShowing());

  // The quiet abusive chip does not have animation and will start the dismiss
  // timer immediately after displaying.
  EXPECT_FALSE(chip.get_chip_button_for_testing()->is_animating());
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  // The dismiss timer is 18 seconds by default. After 17 seconds, the chip
  // should be there.
  task_environment()->AdvanceClock(kQuietChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  // Wait 2 more seconds for the dismiss timer to finish.
  task_environment()->AdvanceClock(base::Seconds(2));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, MultiClickOnQuietChipAbusiveTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications});
  PermissionChip chip;
  chip.SetupChip(
      std::make_unique<PermissionQuietChip>(browser(), &delegate, false));

  EXPECT_FALSE(chip.IsBubbleShowing());

  // The quiet abusive chip does not have animation and will start the dismiss
  // timer immediately after displaying.
  EXPECT_FALSE(chip.get_chip_button_for_testing()->is_animating());
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  // The dismiss timer is 18 seconds by default. After 17 seconds, the chip
  // should be there.
  task_environment()->AdvanceClock(kQuietChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  ClickOnChip(chip);
  EXPECT_TRUE(chip.IsBubbleShowing());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());
  EXPECT_TRUE(chip.IsBubbleShowing());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  // The seconds click on the chip hides the popup bubble.
  ClickOnChip(chip);
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // After the second click, only dismiss timer should be active.
  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kQuietChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // The third click on the chip opens the popup bubble again.
  ClickOnChip(chip);
  EXPECT_TRUE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip.is_dismiss_timer_running_for_testing());

  ClickOnChip(chip);
  EXPECT_FALSE(chip.IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip.is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip.is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kQuietChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}
