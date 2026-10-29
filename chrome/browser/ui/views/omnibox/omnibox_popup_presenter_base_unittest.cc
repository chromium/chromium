// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_webui_creation.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

// Stands in for a presenter's deferred creation feature.
BASE_FEATURE(kTestDeferWebUIContentCreation, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

class TestOmniboxPopupPresenter : public OmniboxPopupPresenterBase {
 public:
  using OmniboxPopupPresenterBase::OmniboxPopupPresenterBase;
  using OmniboxPopupPresenterBase::OnPromptRemoved;

  std::optional<base::TimeDelta> ShouldDeferUntilVisualStateReady()
      const override {
    return std::nullopt;
  }
  bool ShouldDebounceResize() const override { return true; }
  bool ShouldApplyHeightWorkarounds() const override { return true; }
  bool ShouldDetachWebContentsOnHide() const override { return true; }
  bool ShouldEvictOnHide() const override { return false; }
  bool ShouldSizeWebViewToPreferredHeight() const override { return true; }

  std::string_view GetPopupMetricPrefix() const override {
    return "TestPrefix";
  }
};

// Presenter whose popup paints its own drop shadow in the page, so the Views
// layer reserves the shadow margin around the content.
class TestShadowOmniboxPopupPresenter : public TestOmniboxPopupPresenter {
 public:
  using TestOmniboxPopupPresenter::TestOmniboxPopupPresenter;

  bool ShouldDrawShadowInWebUI() const override { return true; }
};

// Presenter that defers its content and never creates it, so that each
// creation attempt can be counted.
class TestCountingDeferredPresenter : public TestOmniboxPopupPresenter {
 public:
  using TestOmniboxPopupPresenter::TestOmniboxPopupPresenter;

  int create_count() const { return create_count_; }

 protected:
  std::unique_ptr<OmniboxPopupWebUIBaseContent> CreateWebUIContent() override {
    ++create_count_;
    return nullptr;
  }

 private:
  int create_count_ = 0;
};

class TestDeferredOmniboxPopupPresenter : public OmniboxPopupPresenterBase {
 public:
  using OmniboxPopupPresenterBase::OmniboxPopupPresenterBase;

  std::optional<base::TimeDelta> ShouldDeferUntilVisualStateReady()
      const override {
    return base::Milliseconds(100);
  }
  bool ShouldDebounceResize() const override { return true; }
  bool ShouldApplyHeightWorkarounds() const override { return true; }
  bool ShouldDetachWebContentsOnHide() const override { return true; }
  bool ShouldEvictOnHide() const override { return false; }
  bool ShouldSizeWebViewToPreferredHeight() const override { return true; }

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
  views::View* GetLocationBarFocusRestoreView() override { return nullptr; }
  bool is_full_webui_omnibox() const override { return false; }
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
    test_widget->MakeCloseSynchronous(
        base::BindOnce(&OmniboxPopupPresenterBase::OnWidgetClosed,
                       base::Unretained(presenter_.get())));
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

  void ShowWidget() { presenter_->ShowWidget(base::TimeTicks::Now()); }

  base::WeakPtr<OmniboxPopupPresenterBase> GetVisualStateWeakPtr(
      OmniboxPopupPresenterBase* presenter) {
    return presenter->visual_state_weak_factory_.GetWeakPtr();
  }

  int GetContentHeight(OmniboxPopupPresenterBase* presenter) {
    return presenter->content_height_;
  }

  using Strategy = OmniboxPopupPresenterBase::WebUIContentCreationStrategy;

  static Strategy GetStrategy() {
    return OmniboxPopupPresenterBase::GetWebUIContentCreationStrategy(
        kTestDeferWebUIContentCreation);
  }

  // Runs the tasks posted to the current thread so far.
  void RunPostedTasks() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
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
  ShowWidget();
  EXPECT_FALSE(presenter_->IsPermissionPromptPreventingClose());

  // Calling `SetPermissionPromptShowing(true)` locks presenter via
  // dismissal mode, ensuring focus-loss events in omnibox are ignored.
  presenter_->SetPermissionPromptShowing(true);
  EXPECT_TRUE(presenter_->IsPermissionPromptPreventingClose());

  // Reset to clean state.
  presenter_->ResetPermissionPromptShowingState();
  EXPECT_FALSE(presenter_->IsPermissionPromptPreventingClose());

  // `OnPromptRemoved` puts it in dismissal mode when shown.
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

  // When the popup widget is hidden externally (e.g. during window teardown),
  // `OnPromptRemoved` should clear `is_prompt_showing_` without entering
  // dismissal mode.
  presenter_->SetPermissionPromptShowing(true);
  widget_ptr_->Hide();
  EXPECT_FALSE(presenter_->IsShown());
  presenter_->OnPromptRemoved();
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

// When the page paints the shadow, the height the renderer reports includes
// the shadow margin the document reserves for it. `content_height_` must stay
// the height of the popup content alone.
TEST_F(OmniboxPopupPresenterBaseTest, ContentHeightExcludesShadowMargin) {
  auto presenter = std::make_unique<TestShadowOmniboxPopupPresenter>(
      nullptr, dummy_delegate_, controller_.get());

  presenter->OnContentHeightChanged(400);

  EXPECT_EQ(GetContentHeight(presenter.get()),
            400 - RoundedOmniboxResultsFrame::GetShadowInsets().height());
}

TEST_F(OmniboxPopupPresenterBaseTest, ContentHeightUnchangedWithoutShadow) {
  auto presenter = std::make_unique<TestOmniboxPopupPresenter>(
      nullptr, dummy_delegate_, controller_.get());

  presenter->OnContentHeightChanged(400);

  EXPECT_EQ(GetContentHeight(presenter.get()), 400);
}

// A height smaller than the shadow margin must not produce a zero or negative
// content height, which would be read as "no content yet".
TEST_F(OmniboxPopupPresenterBaseTest, ContentHeightClampedBelowShadowMargin) {
  auto presenter = std::make_unique<TestShadowOmniboxPopupPresenter>(
      nullptr, dummy_delegate_, controller_.get());

  presenter->OnContentHeightChanged(1);

  EXPECT_EQ(GetContentHeight(presenter.get()), 1);
}

// When ash hides the popup on switching users, and the omnibox closes the popup
// while it is already hidden externally, the widget must be released so ash
// cannot re-show it on switching back.
TEST_F(OmniboxPopupPresenterBaseTest, DiscardsWidgetWhenHiddenExternally) {
  ShowWidget();
  ASSERT_TRUE(widget_ptr_->IsVisible());

  // Ash hides the popup on switching users.
  widget_ptr_->Hide();

  // Omnibox closes the popup while it was already hidden externally.
  presenter_->Hide();

  // The widget should be discarded so external window management cannot
  // re-show it.
  EXPECT_EQ(presenter_->GetWidget(), nullptr);
  EXPECT_FALSE(presenter_->IsShown());
}

// When closed normally while visible, the widget should be hidden but kept
// alive for reuse.
TEST_F(OmniboxPopupPresenterBaseTest, RetainsWidgetWhenHiddenNormally) {
  ShowWidget();
  ASSERT_TRUE(widget_ptr_->IsVisible());

  presenter_->Hide();

  EXPECT_NE(presenter_->GetWidget(), nullptr);
  EXPECT_FALSE(presenter_->GetWidget()->IsVisible());
  EXPECT_FALSE(presenter_->IsShown());
}

// A redundant `Hide()` while already closed must not discard the cached widget.
TEST_F(OmniboxPopupPresenterBaseTest, RedundantHideDoesNotDiscardWidget) {
  ShowWidget();
  presenter_->Hide();
  ASSERT_NE(presenter_->GetWidget(), nullptr);

  presenter_->Hide();

  EXPECT_NE(presenter_->GetWidget(), nullptr);
  EXPECT_FALSE(presenter_->IsShown());
}

// Repeated browser window presentations before the creation task runs post
// only that one task, and none of them creates the content inline.
TEST_F(OmniboxPopupPresenterBaseTest, BrowserWindowPresentationPostsOneTask) {
  auto presenter = std::make_unique<TestCountingDeferredPresenter>(
      nullptr, dummy_delegate_, controller_.get());

  presenter->OnBrowserWindowPresented();
  presenter->OnBrowserWindowPresented();
  EXPECT_EQ(presenter->create_count(), 0);
  RunPostedTasks();
  presenter->OnBrowserWindowPresented();
  RunPostedTasks();

  EXPECT_EQ(presenter->create_count(), 1);
}

// Without deferral, content is created in the constructor.
TEST_F(OmniboxPopupPresenterBaseTest, CreationStrategyIsEagerByDefault) {
  EXPECT_EQ(GetStrategy(), Strategy::kEager);
}

// With the deferral feature, content is created after the window's first
// presentation.
TEST_F(OmniboxPopupPresenterBaseTest,
       CreationStrategyIsAfterWindowPresentationWithDeferFeature) {
  base::test::ScopedFeatureList features(kTestDeferWebUIContentCreation);
  EXPECT_EQ(GetStrategy(), Strategy::kAfterWindowPresentation);
}

// On-demand creation, as browser tests set it up, takes precedence over the
// deferral feature.
TEST_F(OmniboxPopupPresenterBaseTest,
       CreationStrategyIsOnDemandEvenWithDeferFeature) {
  base::test::ScopedFeatureList features(kTestDeferWebUIContentCreation);
  base::AutoReset<bool> on_demand =
      omnibox::SetCreatePopupWebUIOnDemandForTesting(true);
  EXPECT_EQ(GetStrategy(), Strategy::kOnDemand);
}
