// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"

#include <memory>
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

#if defined(USE_AURA)
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#endif

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
#if defined(USE_AURA)
    host_resolver()->AddRule("*", "127.0.0.1");
#endif
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

#if defined(USE_AURA)
// Regression test for crbug.com/575312381: an already-open Autofill popup must
// hide when an HTML form popup opens in the same WebContents, even at
// non-overlapping coordinates.
IN_PROC_BROWSER_TEST_F(PopupBaseViewInteractiveUiTest,
                       HideOnHtmlFormPopupShownAfterPopup) {
  content::WebContents* web_contents = mock_delegate().GetWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<input id='d' type='date' "
                      "style='position:fixed;left:0;top:0'>")));

  gfx::Rect web_bounds = web_contents->GetViewBounds();
  gfx::RectF bounds(web_bounds.right() - 100, web_bounds.bottom() - 100, 10,
                    10);
  ON_CALL(mock_delegate(), element_bounds()).WillByDefault(ReturnRef(bounds));

  ASSERT_TRUE(ShowView());
  ASSERT_TRUE(view()->GetWidget());
  views::test::WidgetDestroyedWaiter destroyed_waiter(view()->GetWidget());

  EXPECT_CALL(mock_delegate(), Hide(_)).Times(AnyNumber());
  EXPECT_CALL(mock_delegate(),
              Hide(SuggestionHidingReason::kHtmlFormPopupOpened))
      .WillOnce([this] { HideView(); });

  content::ShowPopupWidgetWaiter waiter(web_contents,
                                        web_contents->GetPrimaryMainFrame());
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "document.getElementById('d').showPicker();"));
  waiter.Wait();
  ASSERT_FALSE(web_contents->GetPopupWidgets().empty());
  ASSERT_TRUE(web_contents->GetPopupWidgets()[0]->IsShowing());

  destroyed_waiter.Wait();
}

// Regression test for crbug.com/575312381: an already-open Autofill popup must
// hide when a cross-site OOPIF opens a <select> popup.
IN_PROC_BROWSER_TEST_F(PopupBaseViewInteractiveUiTest,
                       HideOnOopifSelectPopupShownAfterPopup) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](net::EmbeddedTestServer* server,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content_type("text/html");
        if (request.GetURL().path() == "/main.html") {
          response->set_content(base::StringPrintf(
              "<iframe src='%s' style='position:fixed;left:0;top:0;"
              "width:200px;height:100px'></iframe>",
              server->GetURL("b.test", "/iframe.html").spec().c_str()));
          return response;
        }
        if (request.GetURL().path() == "/iframe.html") {
          response->set_content(
              "<select id='s'><option>One</option><option>Two</option>"
              "</select>");
          return response;
        }
        return nullptr;
      },
      embedded_test_server()));
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* web_contents = mock_delegate().GetWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/main.html")));

  content::RenderFrameHost* subframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  ASSERT_TRUE(subframe->IsCrossProcessSubframe());
  // The OOPIF's renderer drops input while its commits are deferred after
  // load, so wait until it has submitted a frame.
  content::WaitForHitTestData(subframe);
  ASSERT_TRUE(
      content::ExecJs(subframe, "document.getElementById('s').focus();"));

  gfx::Rect web_bounds = web_contents->GetViewBounds();
  gfx::RectF bounds(web_bounds.right() - 100, web_bounds.bottom() - 100, 10,
                    10);
  ON_CALL(mock_delegate(), element_bounds()).WillByDefault(ReturnRef(bounds));

  ASSERT_TRUE(ShowView());
  ASSERT_TRUE(view()->GetWidget());
  views::test::WidgetDestroyedWaiter destroyed_waiter(view()->GetWidget());

  EXPECT_CALL(mock_delegate(), Hide(_)).Times(AnyNumber());
  EXPECT_CALL(mock_delegate(),
              Hide(SuggestionHidingReason::kHtmlFormPopupOpened))
      .WillOnce([this] { HideView(); });

  content::ShowPopupWidgetWaiter waiter(web_contents, subframe);
  input::NativeWebKeyboardEvent space_event(
      blink::WebKeyboardEvent::Type::kChar, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  space_event.text[0] = ' ';
  subframe->GetRenderWidgetHost()->ForwardKeyboardEvent(space_event);
  waiter.Wait();
  ASSERT_FALSE(web_contents->GetPopupWidgets().empty());
  ASSERT_TRUE(web_contents->GetPopupWidgets()[0]->IsShowing());

  destroyed_waiter.Wait();
}
#endif  // defined(USE_AURA)

}  // namespace
}  // namespace autofill
