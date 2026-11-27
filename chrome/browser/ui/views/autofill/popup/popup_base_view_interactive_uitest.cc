// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"

#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/popup_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class MockAutofillPopupViewDelegate : public AutofillPopupViewDelegate {
 public:
  MockAutofillPopupViewDelegate() = default;
  ~MockAutofillPopupViewDelegate() override = default;

  MOCK_METHOD(void, Hide, (SuggestionHidingReason), (override));
  MOCK_METHOD(void, ViewDestroyed, (), (override));

  MOCK_METHOD(gfx::NativeView, container_view, (), (const, override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (const, override));
  MOCK_METHOD(const gfx::RectF&, element_bounds, (), (const, override));
  MOCK_METHOD(PopupAnchorType, anchor_type, (), (const, override));
  MOCK_METHOD(base::i18n::TextDirection,
              GetElementTextDirection,
              (),
              (const, override));

  base::WeakPtr<AutofillPopupViewDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAutofillPopupViewDelegate> weak_ptr_factory_{this};
};

class TestPopupBaseView : public PopupBaseView {
 public:
  TestPopupBaseView(base::WeakPtr<AutofillPopupViewDelegate> delegate,
                    views::Widget* parent_widget)
      : PopupBaseView(std::move(delegate), parent_widget) {}

  bool Show() { return DoShow(); }
  void Hide() { DoHide(); }

 protected:
  ~TestPopupBaseView() override = default;
};

class PopupBaseViewInteractiveUiTestBase : public InProcessBrowserTest {
 public:
  PopupBaseViewInteractiveUiTestBase() = default;
  PopupBaseViewInteractiveUiTestBase(
      const PopupBaseViewInteractiveUiTestBase&) = delete;
  PopupBaseViewInteractiveUiTestBase& operator=(
      const PopupBaseViewInteractiveUiTestBase&) = delete;
  ~PopupBaseViewInteractiveUiTestBase() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
    web_contents->Focus();

    gfx::NativeView native_view = web_contents->GetNativeView();
    ON_CALL(mock_delegate_, container_view())
        .WillByDefault(Return(native_view));
    ON_CALL(mock_delegate_, GetWebContents())
        .WillByDefault(Return(web_contents));

    view_ =
        new TestPopupBaseView(mock_delegate_.GetWeakPtr(),
                              views::Widget::GetWidgetForNativeWindow(
                                  browser()->GetWindow()->GetNativeWindow()));
  }

  void TearDownOnMainThread() override { view_ = nullptr; }

 protected:
  bool ShowView() { return view_->Show(); }
  void HideView() { std::exchange(view_, nullptr)->Hide(); }
  MockAutofillPopupViewDelegate& mock_delegate() { return mock_delegate_; }
  TestPopupBaseView* view() { return view_.get(); }

 private:
  NiceMock<MockAutofillPopupViewDelegate> mock_delegate_;
  raw_ptr<TestPopupBaseView> view_ = nullptr;
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
};

class PopupBaseViewInteractiveUiTest
    : public PopupBaseViewInteractiveUiTestBase {
 public:
  PopupBaseViewInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        {features::kAutofillPopupUseDeleteSoon,
         features::kAutofillPopupDisallowSimultaneousHtmlFormPopup},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PopupBaseViewInteractiveUiTestFeatureDisabled
    : public PopupBaseViewInteractiveUiTestBase {
 public:
  PopupBaseViewInteractiveUiTestFeatureDisabled() {
    feature_list_.InitWithFeatures(
        {features::kAutofillPopupUseDeleteSoon},
        {features::kAutofillPopupDisallowSimultaneousHtmlFormPopup});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Regression test for crbug.com/575312381: an Autofill popup must not open
// while an HTML form popup is already showing in the same `WebContents`, even
// at non-overlapping coordinates.
IN_PROC_BROWSER_TEST_F(PopupBaseViewInteractiveUiTest,
                       DoesNotShowWhenHtmlFormPopupAlreadyOpen) {
  content::WebContents* web_contents = mock_delegate().GetWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<input id='d' type='date' "
                      "style='position:fixed;left:0;top:0'>")));

  content::ShowPopupWidgetWaiter waiter(web_contents,
                                        web_contents->GetPrimaryMainFrame());
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "document.getElementById('d').showPicker();"));
  waiter.Wait();
  ASSERT_FALSE(web_contents->GetPopupWidgets().empty());
  ASSERT_TRUE(web_contents->GetPopupWidgets()[0]->IsShowing());

  gfx::Rect web_bounds = web_contents->GetViewBounds();
  gfx::RectF bounds(web_bounds.right() - 100, web_bounds.bottom() - 100, 10,
                    10);
  ON_CALL(mock_delegate(), element_bounds()).WillByDefault(ReturnRef(bounds));

  std::optional<views::test::WidgetDestroyedWaiter> destroyed_waiter;
  EXPECT_CALL(mock_delegate(), Hide(_)).Times(AnyNumber());
  EXPECT_CALL(mock_delegate(),
              Hide(SuggestionHidingReason::kOverlappingWithHtmlFormPopup))
      .WillOnce([&] {
        ASSERT_TRUE(view()->GetWidget());
        destroyed_waiter.emplace(view()->GetWidget());
        HideView();
      });

  EXPECT_FALSE(ShowView());
  ASSERT_TRUE(destroyed_waiter);
  destroyed_waiter->Wait();
}

// Verifies that when `kAutofillPopupDisallowSimultaneousHtmlFormPopup` is
// disabled, a non-overlapping HTML form popup does not prevent showing.
IN_PROC_BROWSER_TEST_F(PopupBaseViewInteractiveUiTestFeatureDisabled,
                       ShowsAtNonOverlappingBoundsWhenFeatureDisabled) {
  content::WebContents* web_contents = mock_delegate().GetWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<input id='d' type='date' "
                      "style='position:fixed;left:0;top:0'>")));

  content::ShowPopupWidgetWaiter waiter(web_contents,
                                        web_contents->GetPrimaryMainFrame());
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "document.getElementById('d').showPicker();"));
  waiter.Wait();
  ASSERT_FALSE(web_contents->GetPopupWidgets().empty());
  ASSERT_TRUE(web_contents->GetPopupWidgets()[0]->IsShowing());

  gfx::Rect web_bounds = web_contents->GetViewBounds();
  gfx::RectF bounds(web_bounds.right() - 100, web_bounds.bottom() - 100, 10,
                    10);
  ON_CALL(mock_delegate(), element_bounds()).WillByDefault(ReturnRef(bounds));
  EXPECT_CALL(mock_delegate(),
              Hide(SuggestionHidingReason::kOverlappingWithHtmlFormPopup))
      .Times(0);

  EXPECT_TRUE(ShowView());
  ASSERT_TRUE(view()->GetWidget());
  EXPECT_FALSE(view()->GetWidget()->IsClosed());

  views::test::WidgetDestroyedWaiter destroyed_waiter(view()->GetWidget());
  HideView();
  destroyed_waiter.Wait();
}

}  // namespace
}  // namespace autofill
