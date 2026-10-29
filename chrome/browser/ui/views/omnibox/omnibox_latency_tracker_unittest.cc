// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_latency_tracker.h"

#include <memory>
#include <optional>

#include "base/auto_reset.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using Event = OmniboxLatencyTracker::Event;
using EndingJourney = OmniboxLatencyTracker::EndingJourney;
using Ui = OmniboxLatencyTracker::Ui;

constexpr char kStartupViewsHistogram[] =
    "Omnibox.Latency.StartupToReady.Views";
constexpr char kStartupFullWebUIHistogram[] =
    "Omnibox.Latency.StartupToReady.FullWebUI";

// Answers whether its tabs are NTPs, which auto-focus the omnibox.
class FakeWebContentsDelegate : public content::WebContentsDelegate {
 public:
  explicit FakeWebContentsDelegate(bool is_ntp) : is_ntp_(is_ntp) {}

  // content::WebContentsDelegate:
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) override {
    return is_ntp_;
  }

 private:
  const bool is_ntp_;
};

// Reports the window and omnibox state that the test sets.
class FakeTrackerDelegate : public OmniboxLatencyTracker::Delegate {
 public:
  void set_awaiting_handoff(bool awaiting) { awaiting_handoff_ = awaiting; }
  void set_window_active(bool active) { window_active_ = active; }

  // OmniboxLatencyTracker::Delegate:
  bool IsAwaitingFullWebUIFocusHandoff() const override {
    return awaiting_handoff_;
  }
  bool IsWindowActive() const override { return window_active_; }

 private:
  bool awaiting_handoff_ = false;
  bool window_active_ = false;
};

class OmniboxLatencyTrackerTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Earlier tests in this process may have shown a window or interrupted the
    // startup.
    OmniboxLatencyTracker::ResetStartupWindowForTesting();
    startup_metric_utils::GetBrowser().ResetSessionForTesting();
    tracker_ = std::make_unique<OmniboxLatencyTracker>(
        window_.GetUnownedUserDataHost(), delegate_);
    ntp_ = CreateTab(ntp_delegate_);
    page_ = CreateTab(page_delegate_);
  }

  void TearDown() override {
    tracker_.reset();
    ntp_.reset();
    page_.reset();
    OmniboxLatencyTracker::ResetStartupWindowForTesting();
    startup_metric_utils::GetBrowser().ResetSessionForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::WebContents> CreateTab(
      content::WebContentsDelegate& delegate) {
    std::unique_ptr<content::WebContents> tab = CreateTestWebContents();
    tab->SetDelegate(&delegate);
    return tab;
  }

  // Shows the window as the startup window, which starts StartupToReady.
  void ShowStartupWindow() {
    tracker().Notify(Event::kWindowShowing);
    ASSERT_TRUE(tracker().HasOngoingJourneyForTesting());
  }

  // Restores the focus on the window's first activation, which focuses the
  // omnibox for `contents`. Returns the journey that this ends.
  std::optional<EndingJourney> RestoreFocusToOmnibox(
      content::WebContents* contents,
      Ui ui = Ui::kFullWebUI) {
    tracker().Notify(Event::kFocusRestoreStarted);
    std::optional<EndingJourney> journey =
        tracker().NotifyOmniboxFocused(ui, contents);
    tracker().Notify(Event::kFocusRestoreEnded);
    return journey;
  }

  OmniboxLatencyTracker& tracker() { return *tracker_; }

  const base::TimeTicks application_start_ =
      base::TimeTicks::Now() - base::Seconds(2);
  const base::AutoReset<std::optional<base::TimeTicks>>
      application_start_override_ =
          OmniboxLatencyTracker::SetApplicationStartForTesting(
              application_start_);
  testing::NiceMock<MockBrowserWindowInterface> window_;
  FakeTrackerDelegate delegate_;
  FakeWebContentsDelegate ntp_delegate_{/*is_ntp=*/true};
  FakeWebContentsDelegate page_delegate_{/*is_ntp=*/false};
  std::unique_ptr<OmniboxLatencyTracker> tracker_;
  std::unique_ptr<content::WebContents> ntp_;
  std::unique_ptr<content::WebContents> page_;
};

TEST_F(OmniboxLatencyTrackerTest, FocusRestoreEndsStartupToReadyOnceForAnNtp) {
  // Arrange
  ShowStartupWindow();

  // Act: the window's first activation focuses the omnibox for its NTP.
  tracker().Notify(Event::kFocusRestoreStarted);
  const std::optional<EndingJourney> journey =
      tracker().NotifyOmniboxFocused(Ui::kFullWebUI, ntp_.get());
  const std::optional<EndingJourney> again =
      tracker().NotifyOmniboxFocused(Ui::kFullWebUI, ntp_.get());
  tracker().Notify(Event::kFocusRestoreEnded);

  // Assert
  ASSERT_TRUE(journey);
  EXPECT_STREQ(journey->histogram_name, kStartupFullWebUIHistogram);
  EXPECT_EQ(journey->start_time, application_start_);
  EXPECT_FALSE(again);
}

TEST_F(OmniboxLatencyTrackerTest, JourneyHistogramMatchesUi) {
  // Arrange
  ShowStartupWindow();

  // Act
  const std::optional<EndingJourney> journey =
      RestoreFocusToOmnibox(ntp_.get(), Ui::kViews);

  // Assert
  ASSERT_TRUE(journey);
  EXPECT_STREQ(journey->histogram_name, kStartupViewsHistogram);
}

TEST_F(OmniboxLatencyTrackerTest, StaticNotifyForwardsToTracker) {
  // Arrange
  OmniboxLatencyTracker::Notify(&window_, Event::kWindowShowing);

  // Act
  OmniboxLatencyTracker::Notify(&window_, Event::kFocusRestoreStarted);
  const std::optional<EndingJourney> journey =
      OmniboxLatencyTracker::NotifyOmniboxFocused(&window_, Ui::kFullWebUI,
                                                  ntp_.get());
  OmniboxLatencyTracker::Notify(&window_, Event::kFocusRestoreEnded);

  // Assert
  EXPECT_EQ(OmniboxLatencyTracker::From(&window_), tracker_.get());
  ASSERT_TRUE(journey);
  EXPECT_EQ(journey->start_time, application_start_);
}

TEST_F(OmniboxLatencyTrackerTest, OnlyFirstShowStartsStartupToReady) {
  // Arrange: the window's journey was dropped after its first show.
  ShowStartupWindow();
  tracker().Notify(Event::kUserFocusedOmnibox);

  // Act: the window is shown again, e.g. after it was hidden.
  tracker().Notify(Event::kWindowShowing);

  // Assert
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest, OnlyFirstWindowShownStartsStartupToReady) {
  // Arrange: a later window.
  testing::NiceMock<MockBrowserWindowInterface> later_window;
  FakeTrackerDelegate later_delegate;
  OmniboxLatencyTracker later_tracker(later_window.GetUnownedUserDataHost(),
                                      later_delegate);

  // Act
  tracker().Notify(Event::kWindowShowing);
  later_tracker.Notify(Event::kWindowShowing);

  // Assert
  EXPECT_TRUE(tracker().HasOngoingJourneyForTesting());
  EXPECT_FALSE(later_tracker.HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest, WindowWithoutTrackerCanBeStartupWindow) {
  // Arrange: a window without a tracker, e.g. a popup window, is shown first.
  testing::NiceMock<MockBrowserWindowInterface> popup_window;
  OmniboxLatencyTracker::Notify(&popup_window, Event::kWindowShowing);

  // Act
  OmniboxLatencyTracker::Notify(&window_, Event::kWindowShowing);

  // Assert
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest, InterruptedStartupDoesNotStartJourney) {
  // Arrange: other UI, e.g. the profile picker, interrupted the startup.
  startup_metric_utils::GetBrowser().SetNonBrowserUIDisplayed();

  // Act
  tracker().Notify(Event::kWindowShowing);

  // Assert
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest, NullApplicationStartDoesNotStartJourney) {
  // Arrange: there's no application start, e.g. on ChromeOS for a window that
  // the user opened.
  const auto no_application_start =
      OmniboxLatencyTracker::SetApplicationStartForTesting(base::TimeTicks());

  // Act
  tracker().Notify(Event::kWindowShowing);

  // Assert
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest, StartupInterruptedAfterShowDropsJourney) {
  // Arrange: other UI, e.g. a message box, interrupts the startup after the
  // window shows.
  ShowStartupWindow();
  startup_metric_utils::GetBrowser().SetNonBrowserUIDisplayed();

  // Act
  const std::optional<EndingJourney> journey =
      RestoreFocusToOmnibox(ntp_.get());

  // Assert
  EXPECT_FALSE(journey);
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest,
       FocusForNonNtpKeepsJourneyUntilAutoFocusEnds) {
  // Arrange
  ShowStartupWindow();

  // Act: the omnibox is focused while the active page isn't an NTP.
  tracker().Notify(Event::kFocusRestoreStarted);
  const std::optional<EndingJourney> for_page =
      tracker().NotifyOmniboxFocused(Ui::kFullWebUI, page_.get());
  const bool kept_for_page = tracker().HasOngoingJourneyForTesting();
  tracker().Notify(Event::kFocusRestoreEnded);

  // Assert: the journey is dropped once the focus restore ends.
  EXPECT_FALSE(for_page);
  EXPECT_TRUE(kept_for_page);
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest, FocusOutsideAutoFocusDropsJourney) {
  // Arrange
  ShowStartupWindow();

  // Act: the omnibox is focused other than by an auto-focus, e.g. with the Tab
  // key.
  const std::optional<EndingJourney> journey =
      tracker().NotifyOmniboxFocused(Ui::kFullWebUI, ntp_.get());

  // Assert: a later auto-focus can't end the journey either.
  EXPECT_FALSE(journey);
  EXPECT_FALSE(RestoreFocusToOmnibox(ntp_.get()));
}

TEST_F(OmniboxLatencyTrackerTest, FocusRestoreWithoutOmniboxFocusDropsJourney) {
  // Arrange
  ShowStartupWindow();

  // Act: the focus restore focuses something else, e.g. the page.
  tracker().Notify(Event::kFocusRestoreStarted);
  tracker().Notify(Event::kFocusRestoreEnded);

  // Assert
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest, HandoffToFullWebUIEndsStartupToReady) {
  // Arrange: the focus restore focuses the Views omnibox, which holds the focus
  // until the Full WebUI omnibox is ready.
  ShowStartupWindow();
  delegate_.set_awaiting_handoff(true);
  tracker().Notify(Event::kFocusRestoreStarted);
  tracker().Notify(Event::kFocusRestoreEnded);
  ASSERT_TRUE(tracker().HasOngoingJourneyForTesting());
  delegate_.set_awaiting_handoff(false);

  // Act: the Full WebUI omnibox is ready and takes the focus over.
  tracker().Notify(Event::kFullWebUIHandoffStarted);
  const std::optional<EndingJourney> journey =
      tracker().NotifyOmniboxFocused(Ui::kFullWebUI, ntp_.get());
  tracker().Notify(Event::kFullWebUIHandoffEnded);

  // Assert
  ASSERT_TRUE(journey);
  EXPECT_STREQ(journey->histogram_name, kStartupFullWebUIHistogram);
  EXPECT_EQ(journey->start_time, application_start_);
}

TEST_F(OmniboxLatencyTrackerTest, HandoffWithoutOmniboxFocusDropsJourney) {
  // Arrange
  ShowStartupWindow();

  // Act: the handoff doesn't show the omnibox focused.
  tracker().Notify(Event::kFullWebUIHandoffStarted);
  tracker().Notify(Event::kFullWebUIHandoffEnded);

  // Assert
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest, ReadyWithoutFocusInActiveWindowDropsJourney) {
  // Arrange: the Views omnibox held the restored focus for the handoff.
  ShowStartupWindow();
  delegate_.set_awaiting_handoff(true);
  tracker().Notify(Event::kFocusRestoreStarted);
  tracker().Notify(Event::kFocusRestoreEnded);
  delegate_.set_awaiting_handoff(false);
  delegate_.set_window_active(true);

  // Act: the user moved the focus away before the Full WebUI omnibox was ready.
  tracker().Notify(Event::kFullWebUIReadyWithoutFocus);

  // Assert
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest, ReadyBeforeActivationKeepsJourney) {
  // Arrange
  ShowStartupWindow();
  delegate_.set_window_active(false);

  // Act: the Full WebUI omnibox is ready before the window's first activation,
  // which then focuses it.
  tracker().Notify(Event::kFullWebUIReadyWithoutFocus);
  const std::optional<EndingJourney> journey =
      RestoreFocusToOmnibox(ntp_.get());

  // Assert
  EXPECT_TRUE(journey);
}

TEST_F(OmniboxLatencyTrackerTest, WindowDeactivationDropsJourney) {
  // Arrange
  ShowStartupWindow();

  // Act: the user switches away before the omnibox is ready.
  tracker().Notify(Event::kWindowDeactivated);

  // Assert
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest,
       WindowDeactivationDuringAutoFocusKeepsJourney) {
  // Arrange
  ShowStartupWindow();

  // Act: the Full WebUI omnibox popup takes the activation from its window
  // while the omnibox is auto-focused.
  tracker().Notify(Event::kFocusRestoreStarted);
  tracker().Notify(Event::kWindowDeactivated);
  const std::optional<EndingJourney> journey =
      tracker().NotifyOmniboxFocused(Ui::kFullWebUI, ntp_.get());
  tracker().Notify(Event::kFocusRestoreEnded);

  // Assert
  EXPECT_TRUE(journey);
}

TEST_F(OmniboxLatencyTrackerTest, WindowShownMinimizedDropsJourney) {
  // Arrange
  ShowStartupWindow();

  // Act
  tracker().Notify(Event::kWindowShownMinimized);

  // Assert
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

TEST_F(OmniboxLatencyTrackerTest, UserFocusDropsJourney) {
  // Arrange
  ShowStartupWindow();

  // Act
  tracker().Notify(Event::kUserFocusedOmnibox);

  // Assert
  EXPECT_FALSE(tracker().HasOngoingJourneyForTesting());
}

}  // namespace
