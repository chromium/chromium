// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"

#include <memory>
#include <optional>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/omnibox/omnibox_popup_native_widget_mac_test_util.h"
#endif

namespace {

class DummyOmniboxPopupPresenterDelegate
    : public OmniboxPopupPresenterDelegate {
 public:
  views::Widget* GetLocationBarWidget() override {
    return location_bar_widget_;
  }
  OmniboxPopupFileSelector* GetOmniboxPopupFileSelector() const override {
    return nullptr;
  }
  OmniboxPopupAimPresenter* GetOmniboxPopupAimPresenter() const override {
    return nullptr;
  }
  views::View* GetLocationBarFocusRestoreView() override { return nullptr; }
  bool is_full_webui_omnibox() const override { return false; }

  void set_location_bar_widget(views::Widget* widget) {
    location_bar_widget_ = widget;
  }

 private:
  raw_ptr<views::Widget> location_bar_widget_ = nullptr;
};

class TestOmniboxPopupFullPresenter : public OmniboxPopupFullPresenter {
 public:
  TestOmniboxPopupFullPresenter(
      OmniboxPopupPresenterDelegate& presenter_delegate,
      OmniboxController* controller)
      : OmniboxPopupFullPresenter(nullptr, presenter_delegate, controller) {}

  using OmniboxPopupFullPresenter::GetShadowMargin;

  int content_height() const { return content_height_; }
  void set_content_height(int height) { content_height_ = height; }
  int synchronize_popup_bounds_count() const {
    return synchronize_popup_bounds_count_;
  }

 protected:
  void SynchronizePopupBounds() override {
    ++synchronize_popup_bounds_count_;
    OmniboxPopupFullPresenter::SynchronizePopupBounds();
  }

 private:
  int synchronize_popup_bounds_count_ = 0;
};

class OmniboxPopupFullPresenterTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    auto client = std::make_unique<TestOmniboxClient>();
    controller_ = std::make_unique<OmniboxController>(std::move(client));

    ResetPresenter();
  }

  void ResetPresenter() {
    widget_ptr_ = nullptr;
    presenter_ = std::make_unique<TestOmniboxPopupFullPresenter>(
        dummy_delegate_, controller_.get());

    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);
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
  std::unique_ptr<TestOmniboxPopupFullPresenter> presenter_;
  raw_ptr<views::Widget> widget_ptr_ = nullptr;
  std::unique_ptr<OmniboxController> controller_;
};

TEST_F(OmniboxPopupFullPresenterTest, ResetsContentHeightOnHideWhenEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kOmniboxFullWebUIHeightWorkarounds);
  ResetPresenter();

  presenter_->set_content_height(400);
  EXPECT_EQ(presenter_->content_height(), 400);

  presenter_->Hide();
  EXPECT_EQ(presenter_->content_height(), 1);
}

TEST_F(OmniboxPopupFullPresenterTest,
       PreservesContentHeightOnHideWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      omnibox::kOmniboxFullWebUIHeightWorkarounds);
  ResetPresenter();

  presenter_->set_content_height(400);
  EXPECT_EQ(presenter_->content_height(), 400);

  presenter_->Hide();
  EXPECT_EQ(presenter_->content_height(), 400);
}

TEST_F(OmniboxPopupFullPresenterTest,
       PreservesContentHeightOnHideWhenSizedToPreferredHeight) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{omnibox::
                                kOmniboxFullWebUISizeWebViewToPreferredHeight},
      /*disabled_features=*/{omnibox::kOmniboxFullWebUIHeightWorkarounds});
  ResetPresenter();

  presenter_->set_content_height(400);
  EXPECT_EQ(presenter_->content_height(), 400);

  presenter_->Hide();
  EXPECT_EQ(presenter_->content_height(), 400);
}

TEST_F(OmniboxPopupFullPresenterTest, ShadowMarginDropsBottomWhenCollapsed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{omnibox::internal::kWebUIOmniboxFullPopup,
                            omnibox::kOmniboxFullWebUIShadow},
      /*disabled_features=*/{});
  ResetPresenter();

  const gfx::Insets full_insets = RoundedOmniboxResultsFrame::GetShadowInsets();
  gfx::Insets collapsed_insets = full_insets;
  collapsed_insets.set_bottom(0);
  const int collapsed_height =
      GetLayoutConstant(LayoutConstant::kLocationBarHeight) +
      FullWebUIOmniboxFrame::GetLocationBarAlignmentInsets().height();

  // Initially collapsed: bottom shadow margin is 0.
  EXPECT_EQ(presenter_->GetShadowMargin(), collapsed_insets);

  // The WebUI's reported height always includes its full vertical shadow
  // padding, which is stripped before deciding whether the popup is collapsed.
  presenter_->OnContentHeightChanged(collapsed_height + full_insets.height());
  EXPECT_EQ(presenter_->content_height(), collapsed_height);
  EXPECT_EQ(presenter_->GetShadowMargin(), collapsed_insets);

  // Expanded: the full shadow margin is reserved.
  presenter_->OnContentHeightChanged(collapsed_height + 200 +
                                     full_insets.height());
  EXPECT_EQ(presenter_->content_height(), collapsed_height + 200);
  EXPECT_EQ(presenter_->GetShadowMargin(), full_insets);

  // Collapsing again drops the bottom margin.
  presenter_->OnContentHeightChanged(collapsed_height + full_insets.height());
  EXPECT_EQ(presenter_->content_height(), collapsed_height);
  EXPECT_EQ(presenter_->GetShadowMargin(), collapsed_insets);
}

// The popup is its own top-level window, which doesn't always move with the
// browser window (e.g. a macOS child window when the browser is resized from
// its top edge). It must re-align itself whenever the browser widget's bounds
// change.
TEST_F(OmniboxPopupFullPresenterTest,
       SynchronizesBoundsWhenBrowserWidgetMoves) {
  std::unique_ptr<views::Widget> browser_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  dummy_delegate_.set_location_bar_widget(browser_widget.get());
  widget_ptr_->Show();

  auto* observer = static_cast<views::WidgetObserver*>(presenter_.get());
  const int initial_count = presenter_->synchronize_popup_bounds_count();

  observer->OnWidgetBoundsChanged(browser_widget.get(),
                                  gfx::Rect(10, 60, 400, 300));
  EXPECT_EQ(presenter_->synchronize_popup_bounds_count(), initial_count + 1);

  // The popup widget's own bounds changes, e.g. from syncing, are ignored.
  observer->OnWidgetBoundsChanged(widget_ptr_, gfx::Rect(10, 60, 400, 40));
  EXPECT_EQ(presenter_->synchronize_popup_bounds_count(), initial_count + 1);

  // A hidden popup isn't synced.
  widget_ptr_->Hide();
  observer->OnWidgetBoundsChanged(browser_widget.get(),
                                  gfx::Rect(10, 80, 400, 280));
  EXPECT_EQ(presenter_->synchronize_popup_bounds_count(), initial_count + 1);

  dummy_delegate_.set_location_bar_widget(nullptr);
}

#if BUILDFLAG(IS_MAC)
// Reports a connected WebUI page handler, which `ReshowPopup()` requires.
class ReadyPopupViewFullWebUI : public OmniboxPopupViewFullWebUI {
 public:
  using OmniboxPopupViewFullWebUI::OmniboxPopupViewFullWebUI;

  bool IsPopupHandlerReady() const override { return true; }
};

// Hosts the presenter in its popup view, as in a browser window, with a draft
// in the omnibox and a browser window that can be moved off the active Space.
class OmniboxPopupFullPresenterReshowTest
    : public OmniboxPopupFullPresenterTest {
 public:
  void SetUp() override {
    OmniboxPopupFullPresenterTest::SetUp();
    browser_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    dummy_delegate_.set_location_bar_widget(browser_widget_.get());
    presenter_ptr_ = presenter_.get();
    popup_view_ = std::make_unique<ReadyPopupViewFullWebUI>(
        /*omnibox_view=*/nullptr, controller_.get(), /*location_bar=*/nullptr,
        std::move(presenter_));
    controller_->edit_model()->SetUserText(u"draft");
    ASSERT_TRUE(controller_->edit_model()->HasDraft());
  }

  void TearDown() override {
    inactive_space_.reset();
    presenter_ptr_ = nullptr;
    widget_ptr_ = nullptr;
    popup_view_.reset();
    dummy_delegate_.set_location_bar_widget(nullptr);
    browser_widget_.reset();
    OmniboxPopupFullPresenterTest::TearDown();
  }

 protected:
  // Shows the browser window as if on a Space that isn't the active one.
  void ShowBrowserWindowOffActiveSpace() {
    inactive_space_.emplace(browser_widget_.get());
    browser_widget_->ShowInactive();
    ASSERT_FALSE(browser_widget_->IsVisibleOnScreen());
  }

  // Makes the browser window's Space the active one.
  void ActivateBrowserWindowSpace() {
    inactive_space_.reset();
    ASSERT_TRUE(browser_widget_->IsVisibleOnScreen());
  }

  bool IsObservingBrowserWidget() const {
    return browser_widget_->HasObserver(
        static_cast<const views::WidgetObserver*>(presenter_ptr_.get()));
  }

  OmniboxPopupState popup_state() const {
    return controller_->popup_state_manager()->popup_state();
  }

  std::unique_ptr<views::Widget> browser_widget_;
  std::unique_ptr<ReadyPopupViewFullWebUI> popup_view_;
  // Owned by `popup_view_`.
  raw_ptr<TestOmniboxPopupFullPresenter> presenter_ptr_ = nullptr;
  std::optional<ScopedSimulateInactiveSpace> inactive_space_;
};

// While the browser window is on another Space, a popup shown for it may not
// be attached to it, and then appears on the active Space. Reopening a draft's
// popup waits for the browser window to be back on screen.
TEST_F(OmniboxPopupFullPresenterReshowTest,
       DefersReshowUntilBrowserWindowIsOnScreen) {
  ShowBrowserWindowOffActiveSpace();

  popup_view_->ReshowPopup();
  EXPECT_EQ(popup_state(), OmniboxPopupState::kNone);

  ActivateBrowserWindowSpace();
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return popup_state() == OmniboxPopupState::kFull; }));
}

TEST_F(OmniboxPopupFullPresenterReshowTest,
       DoesNotDeferReshowWhileBrowserWindowIsOnScreen) {
  browser_widget_->ShowInactive();
  ASSERT_TRUE(browser_widget_->IsVisibleOnScreen());

  popup_view_->ReshowPopup();
  EXPECT_EQ(popup_state(), OmniboxPopupState::kFull);
}

// If the draft is gone by the time the browser window is back on screen,
// nothing reopens, and nothing is left to wait for.
TEST_F(OmniboxPopupFullPresenterReshowTest,
       StopsObservingBrowserWindowWhenDraftIsGone) {
  ShowBrowserWindowOffActiveSpace();
  popup_view_->ReshowPopup();
  ASSERT_TRUE(IsObservingBrowserWidget());

  controller_->edit_model()->SetUserText(u"");
  ASSERT_FALSE(controller_->edit_model()->HasDraft());
  ActivateBrowserWindowSpace();
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !IsObservingBrowserWidget(); }));

  EXPECT_EQ(popup_state(), OmniboxPopupState::kNone);
}

// A modal dialog that closed a draft's popup reopens it once the dialog
// closes. The browser window coming back on screen must not reopen it sooner.
TEST_F(OmniboxPopupFullPresenterReshowTest,
       DoesNotReshowWhileModalDialogIsPending) {
  // A file dialog opens over the draft's popup, which closes for it.
  widget_ptr_->ShowInactive();
  controller_->popup_state_manager()->SetPopupState(OmniboxPopupState::kFull);
  presenter_ptr_->OnFileDialogOpened();
  // Run the blur that the dialog posted.
  base::RunLoop blur_run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, blur_run_loop.QuitClosure());
  blur_run_loop.Run();
  controller_->popup_state_manager()->SetPopupState(OmniboxPopupState::kNone);
  presenter_ptr_->Hide();

  ShowBrowserWindowOffActiveSpace();
  popup_view_->ReshowPopup();
  ASSERT_EQ(popup_state(), OmniboxPopupState::kNone);

  ActivateBrowserWindowSpace();

  EXPECT_EQ(popup_state(), OmniboxPopupState::kNone);
  // Still observed, to reopen the popup once the dialog closes.
  EXPECT_TRUE(IsObservingBrowserWidget());
}
#endif  // BUILDFLAG(IS_MAC)
}  // namespace
