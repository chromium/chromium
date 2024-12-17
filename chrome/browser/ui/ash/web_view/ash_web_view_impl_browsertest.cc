// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/web_view/ash_web_view_impl.h"

#include <memory>
#include <string>

#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace {

using ash::AshWebView;
using ash::AshWebViewFactory;

// Macros ----------------------------------------------------------------------

#define EXPECT_PREFERRED_SIZE(web_view_, expected_preferred_size_)         \
  {                                                                        \
    MockViewObserver mock;                                                 \
    base::ScopedObservation<views::View, views::ViewObserver> observation{ \
        &mock};                                                            \
    observation.Observe(static_cast<views::View*>(web_view_));             \
                                                                           \
    base::RunLoop run_loop;                                                \
    EXPECT_CALL(mock, OnViewPreferredSizeChanged)                          \
        .WillOnce(testing::Invoke([&](views::View* view) {                 \
          EXPECT_EQ(expected_preferred_size_, view->GetPreferredSize());   \
          run_loop.QuitClosure().Run();                                    \
        }));                                                               \
    run_loop.Run();                                                        \
  }

#define EXPECT_DID_STOP_LOADING(web_view_)                                     \
  {                                                                            \
    MockAshWebViewObserver mock;                                               \
    base::ScopedObservation<AshWebView, AshWebView::Observer> obs{&mock};      \
    obs.Observe(web_view_);                                                    \
                                                                               \
    base::RunLoop run_loop;                                                    \
    EXPECT_CALL(mock, DidStopLoading).WillOnce(testing::Invoke([&run_loop]() { \
      run_loop.QuitClosure().Run();                                            \
    }));                                                                       \
    run_loop.Run();                                                            \
  }

#define EXPECT_DID_SUPPRESS_NAVIGATION(web_view_, expected_url_,          \
                                       expected_disposition_,             \
                                       expected_from_user_gesture_)       \
  {                                                                       \
    MockAshWebViewObserver mock;                                          \
    base::ScopedObservation<AshWebView, AshWebView::Observer> obs{&mock}; \
    obs.Observe(web_view_);                                               \
                                                                          \
    base::RunLoop run_loop;                                               \
    EXPECT_CALL(mock, DidSuppressNavigation)                              \
        .WillOnce(testing::Invoke([&](const GURL& url,                    \
                                      WindowOpenDisposition disposition,  \
                                      bool from_user_gesture) {           \
          EXPECT_EQ(expected_url_, url);                                  \
          EXPECT_EQ(expected_disposition_, disposition);                  \
          EXPECT_EQ(expected_from_user_gesture_, from_user_gesture);      \
          run_loop.QuitClosure().Run();                                   \
        }));                                                              \
    run_loop.Run();                                                       \
  }

#define EXPECT_DID_CHANGE_CAN_GO_BACK(web_view_, expected_can_go_back_)   \
  {                                                                       \
    MockAshWebViewObserver mock;                                          \
    base::ScopedObservation<AshWebView, AshWebView::Observer> obs{&mock}; \
    obs.Observe(web_view_);                                               \
                                                                          \
    base::RunLoop run_loop;                                               \
    EXPECT_CALL(mock, DidChangeCanGoBack)                                 \
        .WillOnce(testing::Invoke([&](bool can_go_back) {                 \
          EXPECT_EQ(expected_can_go_back_, can_go_back);                  \
          run_loop.QuitClosure().Run();                                   \
        }));                                                              \
    run_loop.Run();                                                       \
  }

// Helpers ---------------------------------------------------------------------

std::unique_ptr<views::Widget> CreateWidget() {
  auto widget = std::make_unique<views::Widget>();

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));
  return widget;
}

GURL CreateDataUrlWithBody(const std::string& body) {
  return GURL(base::StringPrintf(R"(data:text/html,
      <html>
        <body>
          <style>
            * {
              margin: 0;
              padding: 0;
            }
          </style>
          %s
        </body>
      </html>
    )",
                                 body.c_str()));
}

GURL CreateDataUrl() {
  return CreateDataUrlWithBody(std::string());
}

// Mocks -----------------------------------------------------------------------

class MockViewObserver : public testing::NiceMock<views::ViewObserver> {
 public:
  // views::ViewObserver:
  MOCK_METHOD(void,
              OnViewPreferredSizeChanged,
              (views::View * view),
              (override));
};

class MockAshWebViewObserver : public testing::NiceMock<AshWebView::Observer> {
 public:
  // AshWebView::Observer:
  MOCK_METHOD(void, DidStopLoading, (), (override));

  MOCK_METHOD(void,
              DidSuppressNavigation,
              (const GURL& url,
               WindowOpenDisposition disposition,
               bool from_user_gesture),
              (override));

  MOCK_METHOD(void, DidChangeCanGoBack, (bool can_go_back), (override));
};

}  // namespace

// AshWebViewImplBrowserTest
// -------------AshWebViewImpl--------------------------------

class AshWebViewImplBrowserTest : public InProcessBrowserTest {
 public:
  AshWebViewImplBrowserTest() = default;
  AshWebViewImplBrowserTest(const AshWebViewImplBrowserTest&) = delete;
  AshWebViewImplBrowserTest& operator=(const AshWebViewImplBrowserTest&) =
      delete;
  ~AshWebViewImplBrowserTest() override = default;
};

// Tests -----------------------------------------------------------------------

// Tests that AshWebViewImpl will automatically update its preferred size
// to match the desired size of its hosted contents.
IN_PROC_BROWSER_TEST_F(AshWebViewImplBrowserTest, ShouldAutoResize) {
  AshWebView::InitParams params;
  params.enable_auto_resize = true;
  params.min_size = gfx::Size(600, 400);
  params.max_size = gfx::Size(800, 600);

  auto widget = CreateWidget();
  AshWebView* web_view =
      widget->SetContentsView(AshWebViewFactory::Get()->Create(params));

  // Verify auto-resizing within min/max bounds.
  web_view->Navigate(
      CreateDataUrlWithBody("<div style='width:700px; height:500px'></div>"));
  EXPECT_PREFERRED_SIZE(web_view, gfx::Size(700, 500));

  // Verify auto-resizing clamps to min bounds.
  web_view->Navigate(
      CreateDataUrlWithBody("<div style='width:0; height:0'></div>"));
  EXPECT_PREFERRED_SIZE(web_view, gfx::Size(600, 400));

  // Verify auto-resizing clamps to max bounds.
  web_view->Navigate(
      CreateDataUrlWithBody("<div style='width:1000px; height:1000px'></div>"));
  EXPECT_PREFERRED_SIZE(web_view, gfx::Size(800, 600));
}

// Tests that AshWebViewImpl will notify DidStopLoading() events.
IN_PROC_BROWSER_TEST_F(AshWebViewImplBrowserTest, ShouldNotifyDidStopLoading) {
  auto widget = CreateWidget();
  AshWebView* web_view = widget->SetContentsView(
      AshWebViewFactory::Get()->Create(AshWebView::InitParams()));

  web_view->Navigate(CreateDataUrl());
  EXPECT_DID_STOP_LOADING(web_view);
}

// Tests that AshWebViewImpl will notify DidSuppressNavigation() events.
IN_PROC_BROWSER_TEST_F(AshWebViewImplBrowserTest,
                       ShouldNotifyDidSuppressNavigation) {
  AshWebView::InitParams params;
  params.suppress_navigation = true;

  auto widget = CreateWidget();
  AshWebView* web_view = widget->SetContentsView(
      AshWebViewFactory::Get()->Create(std::move(params)));

  web_view->Navigate(CreateDataUrlWithBody(R"(
      <script>
        // Wait until window has finished loading.
        window.addEventListener("load", () => {

          // Perform simple click on an anchor within the same target.
          const anchor = document.createElement("a");
          anchor.href = "https://google.com/";
          anchor.click();

          // Wait for first click event to be flushed.
          setTimeout(() => {

            // Perform simple click on an anchor with "_blank" target.
            const anchor = document.createElement("a");
            anchor.href = "https://assistant.google.com/";
            anchor.target = "_blank";
            anchor.click();
          }, 0);
        });
      </script>
    )"));

  // Expect suppression of the first click event.
  EXPECT_DID_SUPPRESS_NAVIGATION(
      web_view, /*url=*/GURL("https://google.com/"),
      /*disposition=*/WindowOpenDisposition::CURRENT_TAB,
      /*from_user_gesture=*/false);

  // Expect suppression of the second click event.
  EXPECT_DID_SUPPRESS_NAVIGATION(
      web_view, /*url=*/GURL("https://assistant.google.com/"),
      /*disposition=*/WindowOpenDisposition::NEW_FOREGROUND_TAB,
      /*from_user_gesture=*/true);
}

// Tests that AshWebViewImpl will notify DidChangeCanGoBack() events.
IN_PROC_BROWSER_TEST_F(AshWebViewImplBrowserTest,
                       ShouldNotifyDidChangeCanGoBack) {
  auto widget = CreateWidget();
  AshWebView* web_view = widget->SetContentsView(
      AshWebViewFactory::Get()->Create(AshWebView::InitParams()));

  web_view->Navigate(CreateDataUrlWithBody("<div>First Page</div>"));
  EXPECT_DID_STOP_LOADING(web_view);

  web_view->Navigate(CreateDataUrlWithBody("<div>Second Page</div>"));
  EXPECT_DID_CHANGE_CAN_GO_BACK(web_view, /*can_go_back=*/true);

  web_view->GoBack();
  EXPECT_DID_CHANGE_CAN_GO_BACK(web_view, /*can_go_back=*/false);
}

IN_PROC_BROWSER_TEST_F(AshWebViewImplBrowserTest,
                       ShouldSetTemporaryZoomLevelToOne) {
  auto widget = CreateWidget();
  AshWebView::InitParams init_params;
  init_params.fix_zoom_level_to_one = true;

  AshWebView* web_view =
      widget->SetContentsView(AshWebViewFactory::Get()->Create(init_params));
  AshWebViewImpl* web_view_impl = static_cast<AshWebViewImpl*>(web_view);

  web_view->Navigate(CreateDataUrlWithBody("Hello"));
  EXPECT_DID_STOP_LOADING(web_view);

  // Confirm that temporary zoom level is set to 1.
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetForWebContents(web_view_impl->web_contents());
  content::RenderFrameHost* render_frame_host =
      web_view_impl->web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(
      zoom_map->UsesTemporaryZoomLevel(render_frame_host->GetGlobalId()));
  EXPECT_DOUBLE_EQ(
      1.0, content::HostZoomMap::GetZoomLevel(web_view_impl->web_contents()));

  // Navigate to a different url to trigger a RenderFrameHost change.
  web_view->Navigate(CreateDataUrlWithBody("World"));
  EXPECT_DID_STOP_LOADING(web_view);
  ASSERT_NE(render_frame_host,
            web_view_impl->web_contents()->GetPrimaryMainFrame());

  // Confirm that temporary zoom level is still applied even if RenderFrameHost
  // gets changed.
  zoom_map =
      content::HostZoomMap::GetForWebContents(web_view_impl->web_contents());
  render_frame_host = web_view_impl->web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(
      zoom_map->UsesTemporaryZoomLevel(render_frame_host->GetGlobalId()));
  EXPECT_DOUBLE_EQ(
      1.0, content::HostZoomMap::GetZoomLevel(web_view_impl->web_contents()));
}
