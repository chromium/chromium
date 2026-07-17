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

  void SetHasLogged(OmniboxPopupPresenterBase* presenter, bool has_logged) {
    presenter->has_logged_content_ready_since_open_ = has_logged;
  }

  void CallOnVisualStateReady(OmniboxPopupPresenterBase* presenter,
                              base::TimeTicks time,
                              bool from_fallback,
                              bool success) {
    presenter->OnVisualStateReady(time, base::TimeTicks(), from_fallback,
                                  success);
  }

  void CallOnVisualStateReadyForMetrics(base::TimeTicks result_ready_time,
                                        bool success) {
    presenter_->OnVisualStateReadyForMetrics(result_ready_time, success);
  }

  void CallOnWidgetClosed() {
    presenter_->OnWidgetClosed(views::Widget::ClosedReason::kUnspecified);
  }

  base::WeakPtr<OmniboxPopupPresenterBase> GetMetricsWeakPtr() {
    return presenter_->metrics_weak_factory_.GetWeakPtr();
  }

  base::WeakPtr<OmniboxPopupPresenterBase> GetVisualStateWeakPtr() {
    return presenter_->visual_state_weak_factory_.GetWeakPtr();
  }
};

TEST_F(OmniboxPopupPresenterBaseTest, InvalidatesCallbacksOnClose) {
  presenter_->Show();
  // Get weak pointers to simulate pending callbacks.
  auto metrics_weak_ptr = GetMetricsWeakPtr();
  auto weak_ptr = GetVisualStateWeakPtr();

  EXPECT_TRUE(metrics_weak_ptr);
  EXPECT_TRUE(weak_ptr);

  // Closing the widget should invalidate the pending callbacks entirely. Clear
  // the test fixture's handle to the widget before it's destroyed by
  // `OnWidgetClosed` to avoid dangling ptr trips.
  widget_ptr_ = nullptr;

  CallOnWidgetClosed();

  EXPECT_FALSE(metrics_weak_ptr);
  EXPECT_FALSE(weak_ptr);
}

TEST_F(OmniboxPopupPresenterBaseTest, InvalidatesCallbacksOnHide) {
  presenter_->Show();
  // Get weak pointers to simulate pending callbacks.
  auto metrics_weak_ptr = GetMetricsWeakPtr();
  auto weak_ptr = GetVisualStateWeakPtr();

  EXPECT_TRUE(metrics_weak_ptr);
  EXPECT_TRUE(weak_ptr);

  // Hiding the popup should invalidate the pending callbacks.
  presenter_->Hide();

  EXPECT_FALSE(metrics_weak_ptr);
  EXPECT_FALSE(weak_ptr);
}

TEST_F(OmniboxPopupPresenterBaseTest, InvalidatesCallbacksOnShow) {
  // Grab weak pointers while the widget is currently hidden.
  auto metrics_weak_ptr = GetMetricsWeakPtr();
  auto weak_ptr = GetVisualStateWeakPtr();

  EXPECT_TRUE(metrics_weak_ptr);
  EXPECT_TRUE(weak_ptr);

  // Showing the popup should invalidate any stale callbacks.
  presenter_->Show();

  EXPECT_FALSE(metrics_weak_ptr);
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

TEST_F(OmniboxPopupPresenterBaseTest, MetricsRecording) {
  base::HistogramTester histogram_tester;

  // The dummy presenter returns "TestPrefix" for GetPopupMetricPrefix.
  base::TimeTicks ready_time = base::TimeTicks::Now() - base::Milliseconds(50);

  // Need to simulate a 'Show' so flags like
  // has_logged_content_ready_since_open_ are cleanly initialized.
  presenter_->Show();

  CallOnVisualStateReadyForMetrics(ready_time, /*success=*/true);

  histogram_tester.ExpectTotalCount("TestPrefix.ResultToContentReadyPerShow",
                                    1);
  histogram_tester.ExpectTotalCount(
      "TestPrefix.ResultToContentReadyOnFirstShow", 1);

  // To increment PerShow, we must Hide and Show again to simulate a new
  // lifecycle loop.
  presenter_->Hide();
  presenter_->Show();
  CallOnVisualStateReadyForMetrics(ready_time, /*success=*/true);

  histogram_tester.ExpectTotalCount("TestPrefix.ResultToContentReadyPerShow",
                                    2);
  histogram_tester.ExpectTotalCount(
      "TestPrefix.ResultToContentReadyOnFirstShow", 1);
}

TEST_F(OmniboxPopupPresenterBaseTest, DeferredMetricsRecording) {
  auto deferred_presenter = std::make_unique<TestDeferredOmniboxPopupPresenter>(
      nullptr, dummy_delegate_, controller_.get());
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.context = GetContext();
  deferred_presenter->set_widget_for_testing(
      CreateTestWidget(std::move(params)));
  SetIsDeferred(deferred_presenter.get(), true);
  SetHasLogged(deferred_presenter.get(), false);
  base::HistogramTester histogram_tester;

  CallOnVisualStateReady(deferred_presenter.get(), base::TimeTicks::Now(),
                         /*from_fallback=*/false, /*success=*/true);

  // Assert early exit metric is recorded.
  histogram_tester.ExpectBucketCount(
      "TestPrefix.ResultToContentReadyEarlyExitReason",
      1 /* kNoResultReadyTime */, 1);
}

TEST_F(OmniboxPopupPresenterBaseTest, TimeoutFallbackPreemptsVisualState) {
  auto deferred_presenter = std::make_unique<TestDeferredOmniboxPopupPresenter>(
      nullptr, dummy_delegate_, controller_.get());
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.context = GetContext();
  deferred_presenter->set_widget_for_testing(
      CreateTestWidget(std::move(params)));
  SetIsDeferred(deferred_presenter.get(), true);
  SetHasLogged(deferred_presenter.get(), false);
  base::HistogramTester histogram_tester;

  // 1) The fallback timer fires first, before the renderer finishes painting.
  CallOnVisualStateReady(deferred_presenter.get(), base::TimeTicks::Now(),
                         /*from_fallback=*/true, /*success=*/false);

  // Assert that we logged the fallback timeout triggering.
  histogram_tester.ExpectBucketCount(
      "TestPrefix.DeferredShowVisualStateReadyFromTimeout", true, 1);
  // Assert that we did not attempt to log the content-ready latency metric,
  // as we must wait for the actual renderer frame to measure true latency.
  // (If the code mistakenly tried to log it here, an early exit reason would be
  // recorded).
  histogram_tester.ExpectTotalCount(
      "TestPrefix.ResultToContentReadyEarlyExitReason", 0);

  // 2) The genuine visual state callback arrives from the renderer later.
  CallOnVisualStateReady(deferred_presenter.get(), base::TimeTicks::Now(),
                         /*from_fallback=*/false, /*success=*/true);

  // Assert that the UI state was not overridden, and no duplicate telemetry
  // was logged for the visual state display.
  histogram_tester.ExpectBucketCount(
      "TestPrefix.DeferredShowVisualStateReadyFromTimeout", true, 1);
  histogram_tester.ExpectBucketCount(
      "TestPrefix.DeferredShowVisualStateReadyFromTimeout", false, 0);

  // Assert that the latency tracking logic finally executed now that the
  // genuine renderer frame arrived. (In this test environment, it hits
  // kNoResultReadyTime).
  histogram_tester.ExpectBucketCount(
      "TestPrefix.ResultToContentReadyEarlyExitReason",
      1 /* kNoResultReadyTime */, 1);
}

TEST_F(OmniboxPopupPresenterBaseTest, RealVisualStatePreemptsTimeoutFallback) {
  auto deferred_presenter = std::make_unique<TestDeferredOmniboxPopupPresenter>(
      nullptr, dummy_delegate_, controller_.get());
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.context = GetContext();
  deferred_presenter->set_widget_for_testing(
      CreateTestWidget(std::move(params)));
  SetIsDeferred(deferred_presenter.get(), true);
  SetHasLogged(deferred_presenter.get(), false);
  base::HistogramTester histogram_tester;

  // 1) The renderer finishes painting and triggers the callback before the
  // timeout.
  CallOnVisualStateReady(deferred_presenter.get(), base::TimeTicks::Now(),
                         /*from_fallback=*/false, /*success=*/true);

  // Assert that we logged a successful visual state display (not from a
  // timeout).
  histogram_tester.ExpectBucketCount(
      "TestPrefix.DeferredShowVisualStateReadyFromTimeout", false, 1);
  // Assert that no timeout was logged.
  histogram_tester.ExpectBucketCount(
      "TestPrefix.DeferredShowVisualStateReadyFromTimeout", true, 0);
  // Assert that we attempted to log latency (hitting the expected early exit
  // reason).
  histogram_tester.ExpectBucketCount(
      "TestPrefix.ResultToContentReadyEarlyExitReason",
      1 /* kNoResultReadyTime */, 1);

  // 2) The fallback timer eventually fires, but its task is effectively
  // ignored.
  CallOnVisualStateReady(deferred_presenter.get(), base::TimeTicks::Now(),
                         /*from_fallback=*/true, /*success=*/false);

  // Assert that the fallback timer safely exited without logging metrics again
  // or overwriting the true telemetry state.
  histogram_tester.ExpectBucketCount(
      "TestPrefix.DeferredShowVisualStateReadyFromTimeout", false, 1);
  histogram_tester.ExpectBucketCount(
      "TestPrefix.DeferredShowVisualStateReadyFromTimeout", true, 0);
  histogram_tester.ExpectBucketCount(
      "TestPrefix.ResultToContentReadyEarlyExitReason",
      1 /* kNoResultReadyTime */, 1);
}
