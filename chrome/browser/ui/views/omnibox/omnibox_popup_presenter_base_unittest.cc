// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

class TestOmniboxPopupPresenter : public OmniboxPopupPresenterBase {
 public:
  using OmniboxPopupPresenterBase::OmniboxPopupPresenterBase;
  using OmniboxPopupPresenterBase::OnPromptRemoved;

  std::optional<base::TimeDelta> ShouldDeferUntilVisualStateReady()
      const override {
    return std::nullopt;
  }
  bool ShouldDetachWebContentsOnHide() const override { return true; }

  std::string_view GetPopupMetricPrefix() const override {
    return "TestPrefix";
  }
};

class TestDeferredOmniboxPopupPresenter : public OmniboxPopupPresenterBase {
 public:
  using OmniboxPopupPresenterBase::OmniboxPopupPresenterBase;

  std::optional<base::TimeDelta> ShouldDeferUntilVisualStateReady()
      const override {
    return base::Milliseconds(100);
  }
  bool ShouldDetachWebContentsOnHide() const override { return true; }

  std::string_view GetPopupMetricPrefix() const override {
    return "TestPrefix";
  }
};

class DummyOmniboxPopupPresenterDelegate
    : public OmniboxPopupPresenterDelegate {
 public:
  views::Widget* GetLocationBarWidget() override { return nullptr; }
  OmniboxPopupFileSelector* GetOmniboxPopupFileSelector() const override {
    return nullptr;
  }
  OmniboxPopupAimPresenter* GetOmniboxPopupAimPresenter() const override {
    return nullptr;
  }
  const views::View* GetLocationBarFocusRestoreView() const override {
    return nullptr;
  }
};

class OmniboxPopupPresenterBaseTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    auto client = std::make_unique<TestOmniboxClient>();
    controller_ = std::make_unique<OmniboxController>(std::move(client));

    presenter_ = std::make_unique<TestOmniboxPopupPresenter>(
        nullptr, dummy_delegate_, controller_.get());

    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    // Use TYPE_WINDOW_FRAMELESS to avoid native title bars and decorations
    // which cause platform-dependent minimum bounds constraints in unit tests.
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    params.context = GetContext();
    auto test_widget = CreateTestWidget(std::move(params));

    widget_ptr_ = test_widget.get();
    presenter_->set_widget_for_testing(std::move(test_widget));
  }

  void TearDown() override {
    widget_ptr_ = nullptr;
    presenter_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  DummyOmniboxPopupPresenterDelegate dummy_delegate_;
  std::unique_ptr<TestOmniboxPopupPresenter> presenter_;

  raw_ptr<views::Widget> widget_ptr_ = nullptr;
  std::unique_ptr<OmniboxController> controller_;

  void SetIsDeferred(OmniboxPopupPresenterBase* presenter, bool is_deferred) {
    presenter->is_deferred_ = is_deferred;
  }

  void CallOnVisualStateReady(OmniboxPopupPresenterBase* presenter,
                              base::TimeTicks time,
                              base::TimeTicks result_ready_time,
                              bool from_fallback,
                              bool success) {
    presenter->OnVisualStateReady(time, result_ready_time, from_fallback,
                                  success);
  }

  void CallOnWidgetClosed() {
    presenter_->OnWidgetClosed(views::Widget::ClosedReason::kUnspecified);
  }

  base::WeakPtr<OmniboxPopupPresenterBase> GetVisualStateWeakPtr(
      OmniboxPopupPresenterBase* presenter) {
    return presenter->visual_state_weak_factory_.GetWeakPtr();
  }
};

TEST_F(OmniboxPopupPresenterBaseTest, InvalidatesCallbacksOnClose) {
  presenter_->Show();
  // Get weak pointers to simulate pending callbacks
  auto weak_ptr = GetVisualStateWeakPtr(presenter_.get());

  EXPECT_TRUE(weak_ptr);

  // Closing the widget should invalidate the pending callbacks entirely. Clear
  // the test fixture's handle to the widget before it's destroyed by
  // `OnWidgetClosed` to avoid dangling ptr trips.
  widget_ptr_ = nullptr;

  CallOnWidgetClosed();

  EXPECT_FALSE(weak_ptr);
}

TEST_F(OmniboxPopupPresenterBaseTest, InvalidatesCallbacksOnHide) {
  presenter_->Show();
  // Get weak pointers to simulate pending callbacks.
  auto weak_ptr = GetVisualStateWeakPtr(presenter_.get());

  EXPECT_TRUE(weak_ptr);

  // Hiding the popup should invalidate the pending callbacks.
  presenter_->Hide();

  EXPECT_FALSE(weak_ptr);
}

TEST_F(OmniboxPopupPresenterBaseTest, InvalidatesCallbacksOnShow) {
  // Grab weak pointers while the widget is currently hidden.
  auto weak_ptr = GetVisualStateWeakPtr(presenter_.get());

  EXPECT_TRUE(weak_ptr);

  // Showing the popup should invalidate any stale callbacks.
  presenter_->Show();

  EXPECT_FALSE(weak_ptr);
}

TEST_F(OmniboxPopupPresenterBaseTest, OpenSmallThenGrowLarger) {
  widget_ptr_->SetBounds(gfx::Rect(0, 0, 300, 50));

  presenter_->OnEmbeddedPermissionDialogChanged(true, gfx::Size(500, 400));
  presenter_->SynchronizePopupBounds();

  EXPECT_EQ(widget_ptr_->GetRestoredBounds().width(), 500);

  widget_ptr_->SetBounds(gfx::Rect(0, 0, 800, 50));
  presenter_->SynchronizePopupBounds();

  EXPECT_EQ(widget_ptr_->GetRestoredBounds().width(), 800);
}

TEST_F(OmniboxPopupPresenterBaseTest, OpenLargeThenShrink) {
  widget_ptr_->SetBounds(gfx::Rect(0, 0, 800, 50));

  presenter_->OnEmbeddedPermissionDialogChanged(true, gfx::Size(500, 400));
  presenter_->SynchronizePopupBounds();

  EXPECT_EQ(widget_ptr_->GetRestoredBounds().width(), 800);

  widget_ptr_->SetBounds(gfx::Rect(0, 0, 200, 50));
  presenter_->SynchronizePopupBounds();

  EXPECT_EQ(widget_ptr_->GetRestoredBounds().width(), 500);
}

// Test the 4 Closure States (Allow, Deny, Out of Focus, Allow Always)
TEST_F(OmniboxPopupPresenterBaseTest, ResetsOnAllClosureStates) {
  auto test_closure = [&](const std::string& action_name) {
    presenter_->OnEmbeddedPermissionDialogChanged(true, gfx::Size(500, 400));
    EXPECT_EQ(presenter_->get_minimum_size(), gfx::Size(500, 400))
        << "Failed to open for " << action_name;

    presenter_->OnEmbeddedPermissionDialogChanged(false, gfx::Size());

    EXPECT_EQ(presenter_->get_minimum_size(), gfx::Size())
        << action_name << " failed to reset size!";
  };

  test_closure("Allow");
  test_closure("Deny/Close");
  test_closure("Out of Focus (Blur)");
  test_closure("Allow Always");
}

TEST_F(OmniboxPopupPresenterBaseTest, PermissionPromptShowingStateAndReset) {
  EXPECT_FALSE(presenter_->IsPermissionPromptPreventingClose());

  // Calling `SetPermissionPromptShowing(true)` locks presenter via
  // dismissal mode, ensuring focus-loss events in omnibox are ignored.
  presenter_->SetPermissionPromptShowing(true);
  EXPECT_TRUE(presenter_->IsPermissionPromptPreventingClose());

  // Reset to clean state.
  presenter_->ResetPermissionPromptShowingState();
  EXPECT_FALSE(presenter_->IsPermissionPromptPreventingClose());

  // `OnPromptRemoved` puts it in dismissal mode.
  presenter_->OnPromptRemoved();
  EXPECT_TRUE(presenter_->IsPermissionPromptPreventingClose());

  // Reset to clean state.
  presenter_->ResetPermissionPromptShowingState();
  EXPECT_FALSE(presenter_->IsPermissionPromptPreventingClose());

  // `OnEmbeddedPermissionDialogChanged(true)` locks presenter.
  presenter_->OnEmbeddedPermissionDialogChanged(true, gfx::Size(500, 400));
  EXPECT_TRUE(presenter_->IsPermissionPromptPreventingClose());

  // Reset to clean state.
  presenter_->ResetPermissionPromptShowingState();
  EXPECT_FALSE(presenter_->IsPermissionPromptPreventingClose());

  // `OnEmbeddedPermissionDialogChanged(false)` puts it in dismissal mode.
  presenter_->OnEmbeddedPermissionDialogChanged(false, gfx::Size());
  EXPECT_TRUE(presenter_->IsPermissionPromptPreventingClose());

  // Resetting clears state.
  presenter_->ResetPermissionPromptShowingState();
  EXPECT_FALSE(presenter_->IsPermissionPromptPreventingClose());
}

TEST_F(OmniboxPopupPresenterBaseTest,
       MetricsLoggedOnGraphicsPipelinePreemption) {
  auto deferred_presenter = std::make_unique<TestDeferredOmniboxPopupPresenter>(
      nullptr, dummy_delegate_, controller_.get());
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.context = GetContext();
  deferred_presenter->set_widget_for_testing(
      CreateTestWidget(std::move(params)));
  SetIsDeferred(deferred_presenter.get(), true);

  auto weak_ptr = GetVisualStateWeakPtr(deferred_presenter.get());
  EXPECT_TRUE(weak_ptr);

  base::HistogramTester histogram_tester;

  base::TimeTicks request_time = base::TimeTicks::Now();
  base::TimeTicks ready_time = base::TimeTicks::Now();

  // 1) The renderer finishes painting and triggers the callback before the
  // timeout.
  CallOnVisualStateReady(deferred_presenter.get(), request_time, ready_time,
                         /*from_fallback=*/false, /*success=*/true);

  // Assert that the pending callbacks were invalidated.
  EXPECT_FALSE(weak_ptr);

  // Assert that we logged a successful visual state display (not from a
  // timeout).
  histogram_tester.ExpectBucketCount("TestPrefix.ContentReady.FromTimeout",
                                     false, 1);
  histogram_tester.ExpectBucketCount(
      "TestPrefix.ContentReady.FromTimeout.FirstShow", false, 1);
  // Assert that no timeout was logged.
  histogram_tester.ExpectBucketCount("TestPrefix.ContentReady.FromTimeout",
                                     true, 0);
  histogram_tester.ExpectBucketCount(
      "TestPrefix.ContentReady.FromTimeout.FirstShow", true, 0);
  histogram_tester.ExpectTotalCount("TestPrefix.ContentReady.Duration", 1);
  histogram_tester.ExpectTotalCount(
      "TestPrefix.ContentReady.Duration.FirstShow", 1);
  histogram_tester.ExpectTotalCount(
      "TestPrefix.ResultToContentReadyEarlyExitReason", 0);
  // Assert that we successfully logged the true telemetry latency!
  histogram_tester.ExpectTotalCount("TestPrefix.ResultToContentReadyPerShow",
                                    1);
  histogram_tester.ExpectTotalCount(
      "TestPrefix.ResultToContentReadyOnFirstShow", 1);
}

TEST_F(OmniboxPopupPresenterBaseTest,
       MetricsLoggedOnFallbackTimeoutPreemption) {
  auto deferred_presenter = std::make_unique<TestDeferredOmniboxPopupPresenter>(
      nullptr, dummy_delegate_, controller_.get());
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.context = GetContext();
  deferred_presenter->set_widget_for_testing(
      CreateTestWidget(std::move(params)));
  SetIsDeferred(deferred_presenter.get(), true);

  auto weak_ptr = GetVisualStateWeakPtr(deferred_presenter.get());
  EXPECT_TRUE(weak_ptr);

  base::HistogramTester histogram_tester;

  base::TimeTicks request_time = base::TimeTicks::Now();
  base::TimeTicks ready_time = base::TimeTicks::Now();

  // 1) The fallback timer wins the race and fires first. The timer executes
  // with `success = false` since it is a timeout.
  CallOnVisualStateReady(deferred_presenter.get(), request_time, ready_time,
                         /*from_fallback=*/true, /*success=*/false);

  // Assert that the pending callbacks were invalidated.
  EXPECT_FALSE(weak_ptr);

  // Assert that we logged the fallback timeout triggering.
  histogram_tester.ExpectBucketCount("TestPrefix.ContentReady.FromTimeout",
                                     true, 1);
  histogram_tester.ExpectBucketCount(
      "TestPrefix.ContentReady.FromTimeout.FirstShow", true, 1);
  histogram_tester.ExpectBucketCount("TestPrefix.ContentReady.FromTimeout",
                                     false, 0);
  histogram_tester.ExpectBucketCount(
      "TestPrefix.ContentReady.FromTimeout.FirstShow", false, 0);
  histogram_tester.ExpectTotalCount("TestPrefix.ContentReady.Duration", 1);
  histogram_tester.ExpectTotalCount(
      "TestPrefix.ContentReady.Duration.FirstShow", 1);
  histogram_tester.ExpectTotalCount("TestPrefix.ResultToContentReadyPerShow",
                                    1);
  histogram_tester.ExpectTotalCount(
      "TestPrefix.ResultToContentReadyOnFirstShow", 1);
}
