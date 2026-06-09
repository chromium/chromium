// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_contents_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_widget_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "content/public/browser/fullscreen_types.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/window_container_type.mojom.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {

class RecordingOpenURLDelegate : public content::WebContentsDelegate {
 public:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    ++call_count_;
    source_ = source;
    disposition_ = params.disposition;
    url_ = params.url;
    return source;
  }

  int call_count() const { return call_count_; }
  content::WebContents* source() const { return source_; }
  WindowOpenDisposition disposition() const { return disposition_; }
  const GURL& url() const { return url_; }

 private:
  int call_count_ = 0;
  raw_ptr<content::WebContents> source_ = nullptr;
  WindowOpenDisposition disposition_ = WindowOpenDisposition::UNKNOWN;
  GURL url_;
};

class RecordingAddNewContentsDelegate : public content::WebContentsDelegate {
 public:
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override {
    ++call_count_;
    source_ = source;
    disposition_ = disposition;
    // Retain ownership of the forwarded contents so the returned pointer stays
    // valid for the duration of the test.
    new_contents_ = std::move(new_contents);
    return new_contents_.get();
  }

  int call_count() const { return call_count_; }
  content::WebContents* source() const { return source_; }
  WindowOpenDisposition disposition() const { return disposition_; }

 private:
  int call_count_ = 0;
  raw_ptr<content::WebContents> source_ = nullptr;
  WindowOpenDisposition disposition_ = WindowOpenDisposition::UNKNOWN;
  std::unique_ptr<content::WebContents> new_contents_;
};

blink::mojom::PictureInPictureWindowOptions MakeDefaultPipOptions() {
  blink::mojom::PictureInPictureWindowOptions opts;
  opts.width = 400;
  opts.height = 300;
  opts.disallow_return_to_opener = false;
  opts.prefer_initial_window_placement = false;
  return opts;
}

}  // namespace

class DocumentPipHostTest : public ChromeViewsTestBase {
 public:
  DocumentPipHostTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // In production, ChromeViewsDelegate honours
    // WidgetDelegate::use_desktop_widget_override() and selects
    // DesktopNativeWidgetAura for the PiP widget. The test ViewsDelegate
    // doesn't check that flag, so enable its equivalent here.
    test_views_delegate()->set_use_desktop_native_widgets(true);

    opener_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    ASSERT_TRUE(opener_web_contents_);

    // In production the opener always has a SecurityStateTabHelper attached (by
    // TabHelpers). DocumentPipFrameView reads it to render the origin chip's
    // security icon and CHECK()s the invariant, so attach it here to satisfy
    // that invariant and exercise the populated security-state path.
    ChromeSecurityStateTabHelper::CreateForWebContents(
        opener_web_contents_.get());

    // Host the opener WebContents inside a test top-level widget so that
    // `opener->GetTopLevelNativeWindow()` returns a real native window -
    // DesktopNativeWidgetAura uses it as `params.parent` when the PiP widget
    // is initialized.
    opener_host_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto* web_view = opener_host_widget_->SetContentsView(
        std::make_unique<views::WebView>(&profile_));
    web_view->SetWebContents(opener_web_contents_.get());
    opener_host_widget_->Show();
  }

  void TearDown() override {
    // Resetting the opener destroys the DocumentPipHost (and its widget)
    // automatically, before views tear-down. Must happen before the host
    // widget that owns the WebView referencing it.
    opener_web_contents_.reset();
    opener_host_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  content::WebContents* opener() { return opener_web_contents_.get(); }

  // Creates a DocumentPipHost attached to the opener with a PiP widget open.
  DocumentPipHost* CreateHostAndOpenPipWindow() {
    DocumentPipHost::CreateForWebContents(opener());
    auto* host = DocumentPipHost::FromWebContents(opener());
    auto child =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    host->CreatePipWidget(std::move(child), MakeDefaultPipOptions());
    return host;
  }

 protected:
  // Must be declared before |profile_| because TestingProfile may post tasks.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<views::Widget> opener_host_widget_;
  std::unique_ptr<content::WebContents> opener_web_contents_;
};

// Verify that CreateForWebContents creates and attaches a host.
TEST_F(DocumentPipHostTest, CreateForWebContents_CreatesHost) {
  EXPECT_FALSE(DocumentPipHost::FromWebContents(opener()));
  CreateHostAndOpenPipWindow();
  EXPECT_TRUE(DocumentPipHost::FromWebContents(opener()));
}

// Calling CreateForWebContents a second time is a no-op.
TEST_F(DocumentPipHostTest, CreateForWebContents_IdempotentWhenExists) {
  DocumentPipHost* first = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(first);

  DocumentPipHost::CreateForWebContents(opener());

  EXPECT_EQ(first, DocumentPipHost::FromWebContents(opener()));
}

// The host should expose the opener, child WebContents, profile, and options.
TEST_F(DocumentPipHostTest, Accessors) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_EQ(opener(), host->GetOpenerWebContents());
  EXPECT_TRUE(host->GetChildWebContents());
  EXPECT_NE(opener(), host->GetChildWebContents());
  EXPECT_TRUE(host->GetProfile());
  EXPECT_EQ(400u, host->GetPipOptions().width);
  EXPECT_EQ(300u, host->GetPipOptions().height);
}

// The host creates a Widget with a DocumentPipWidgetDelegate.
TEST_F(DocumentPipHostTest, WidgetIsCreated) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  views::Widget* w = host->GetWidget();
  ASSERT_TRUE(w);
  EXPECT_FALSE(w->IsClosed());
}

// Regression test for crbug.com/519833771: opening the PiP window builds the
// DocumentPipFrameView, which reads the opener's SecurityStateTabHelper to
// render the origin chip security icon. With the helper attached (the
// production invariant), this populated path must complete without crashing.
TEST_F(DocumentPipHostTest, OpenPipWindow_PopulatesSecurityStateWithoutCrash) {
  ASSERT_TRUE(SecurityStateTabHelper::FromWebContents(opener()));

  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  views::Widget* widget = host->GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->non_client_view()->frame_view());
}

// After CreatePipWidget(), the child WebContents is hosted in the
// DocumentPipContentsView (a views::WebView).
TEST_F(DocumentPipHostTest, ChildWebContentsInWebView) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  auto* delegate = static_cast<DocumentPipWidgetDelegate*>(
      host->GetWidget()->widget_delegate());
  ASSERT_TRUE(delegate);

  auto* contents_view = delegate->GetDocumentPipContentsView();
  ASSERT_TRUE(contents_view);
  EXPECT_EQ(host->GetChildWebContents(), contents_view->web_contents());
}

// The widget delegate has the correct properties.
TEST_F(DocumentPipHostTest, WidgetDelegateProperties) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  auto* delegate = host->GetWidget()->widget_delegate();
  ASSERT_TRUE(delegate);

  EXPECT_TRUE(delegate->CanResize());
  EXPECT_FALSE(delegate->CanMaximize());
  EXPECT_FALSE(delegate->CanMinimize());
  EXPECT_FALSE(delegate->CanFullscreen());
}

// GetDisplayMode returns kPictureInPicture for the child WebContents.
TEST_F(DocumentPipHostTest, GetDisplayMode_ReturnsPictureInPicture) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_EQ(blink::mojom::DisplayMode::kPictureInPicture,
            host->GetDisplayMode(host->GetChildWebContents()));
}

// When the child requests closure, the host tears down the widget (and with it
// the child WebContents) but stays alive on the opener.
TEST_F(DocumentPipHostTest, CloseContents_TearsDownWidgetAndChild) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);
  content::WebContents* child = host->GetChildWebContents();
  content::WebContentsDestroyedWatcher child_destroyed_watcher(child);

  // Simulate the child requesting its own closure.
  host->CloseContents(child);

  // The host remains attached to the opener.
  EXPECT_EQ(host, DocumentPipHost::FromWebContents(opener()));
  // But the widget and child WebContents are gone.
  EXPECT_EQ(nullptr, host->GetWidget());
  EXPECT_EQ(nullptr, host->GetChildWebContents());
  EXPECT_TRUE(child_destroyed_watcher.IsDestroyed());
}

// Destroying the opener WebContents destroys the host cleanly.
// DocumentPipHost is a WebContentsUserData, so it is automatically destroyed
// when the opener WebContents is destroyed. The destructor calls
// ClosePipWindow().
TEST_F(DocumentPipHostTest, OpenerDestroyed_HostClosedCleanly) {
  CreateHostAndOpenPipWindow();
  ASSERT_TRUE(DocumentPipHost::FromWebContents(opener()));

  // Destroy the opener; the host is destroyed as UserData without crashing.
  opener_web_contents_.reset();
}

// After the PiP window is closed, CreatePipWidget() can be called again with a
// new child WebContents to re-open the PiP window.
TEST_F(DocumentPipHostTest, ReopenAfterClose) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  // Close the first PiP window.
  host->CloseContents(host->GetChildWebContents());
  EXPECT_EQ(nullptr, host->GetWidget());
  EXPECT_EQ(nullptr, host->GetChildWebContents());

  // Re-open with a new child WebContents.
  auto new_child =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  host->CreatePipWidget(std::move(new_child), MakeDefaultPipOptions());

  EXPECT_TRUE(host->GetWidget());
  EXPECT_TRUE(host->GetChildWebContents());
}

// Regression test for a leak of the WidgetDelegate: with CLIENT_OWNS_WIDGET
// and without SetOwnedByWidget(), the Widget never deletes its delegate, so
// the host must own and destroy it. Verify the delegate is actually destroyed
// (not just unlinked) both when the PiP window is closed and when the host
// itself goes away.
TEST_F(DocumentPipHostTest, ClosePipWindow_DestroysWidgetDelegate) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  auto* delegate = static_cast<DocumentPipWidgetDelegate*>(
      host->GetWidget()->widget_delegate());
  ASSERT_TRUE(delegate);
  base::WeakPtr<DocumentPipWidgetDelegate> delegate_weak =
      delegate->GetWeakPtr();

  host->CloseContents(host->GetChildWebContents());

  EXPECT_FALSE(delegate_weak);
}

TEST_F(DocumentPipHostTest, OpenerDestroyed_DestroysWidgetDelegate) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  auto* delegate = static_cast<DocumentPipWidgetDelegate*>(
      host->GetWidget()->widget_delegate());
  ASSERT_TRUE(delegate);
  base::WeakPtr<DocumentPipWidgetDelegate> delegate_weak =
      delegate->GetWeakPtr();

  // Destroying the opener WebContents destroys the host (it is UserData),
  // which in turn must destroy the delegate.
  opener_web_contents_.reset();

  EXPECT_FALSE(delegate_weak);
}

// --- WebContentsDelegate surface tests ---

// IsWebContentsCreationOverridden returns false - popups are forwarded to the
// opener in AddNewContents(), not blocked at creation.
TEST_F(DocumentPipHostTest, IsWebContentsCreationOverridden_ReturnsFalse) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_FALSE(host->IsWebContentsCreationOverridden(
      /*opener=*/nullptr, /*source_site_instance=*/nullptr,
      content::mojom::WindowContainerType::NORMAL, GURL(), std::string(),
      GURL()));
}

// AddNewContents forwards to the opener's delegate. If the opener has no
// delegate, the popup is blocked.
TEST_F(DocumentPipHostTest, AddNewContents_BlocksWhenOpenerHasNoDelegate) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  // The test opener has no delegate set. Popup should be blocked.
  bool was_blocked = false;
  auto popup =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  host->AddNewContents(host->GetChildWebContents(), std::move(popup), GURL(),
                       WindowOpenDisposition::NEW_POPUP,
                       blink::mojom::WindowFeatures(), /*user_gesture=*/false,
                       &was_blocked);
  EXPECT_TRUE(was_blocked);
}

// Same-window navigations are not allowed inside standalone Document PiP. The
// PiP window is closed instead of navigating either the PiP or opener contents.
TEST_F(DocumentPipHostTest, OpenURLFromTab_CurrentTabClosesPipWindow) {
  RecordingOpenURLDelegate opener_delegate;
  opener()->SetDelegate(&opener_delegate);
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);
  content::WebContents* child = host->GetChildWebContents();
  content::WebContentsDestroyedWatcher child_destroyed_watcher(child);

  content::OpenURLParams params(
      GURL("https://example.test/"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/true);
  EXPECT_EQ(nullptr, host->OpenURLFromTab(child, params, base::NullCallback()));

  EXPECT_EQ(0, opener_delegate.call_count());
  EXPECT_EQ(nullptr, host->GetWidget());
  EXPECT_EQ(nullptr, host->GetChildWebContents());
  EXPECT_TRUE(child_destroyed_watcher.IsDestroyed());
  opener()->SetDelegate(nullptr);
}

// Cross-window navigations from PiP are routed to the opener delegate.
TEST_F(DocumentPipHostTest, OpenURLFromTab_NewForegroundTabRoutesToOpener) {
  RecordingOpenURLDelegate opener_delegate;
  opener()->SetDelegate(&opener_delegate);
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);
  content::WebContents* child = host->GetChildWebContents();
  const GURL url("https://example.test/");

  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/true);
  EXPECT_EQ(opener(),
            host->OpenURLFromTab(child, params, base::NullCallback()));

  EXPECT_EQ(1, opener_delegate.call_count());
  EXPECT_EQ(opener(), opener_delegate.source());
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
            opener_delegate.disposition());
  EXPECT_EQ(url, opener_delegate.url());
  EXPECT_TRUE(host->GetWidget());
  EXPECT_TRUE(host->GetChildWebContents());
  opener()->SetDelegate(nullptr);
}

// GetFullscreenState returns a default (non-fullscreen) state. This is a
// hot-path method called ~456 times per session.
TEST_F(DocumentPipHostTest, GetFullscreenState_ReturnsNonFullscreen) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  content::FullscreenState state =
      host->GetFullscreenState(host->GetChildWebContents());
  EXPECT_EQ(content::FullscreenMode::kWindowed, state.target_mode);
}

// IsFullscreenForTabOrPending returns false. Another hot-path method.
TEST_F(DocumentPipHostTest, IsFullscreenForTabOrPending_ReturnsFalse) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_FALSE(host->IsFullscreenForTabOrPending(host->GetChildWebContents()));
}

// CanEnterFullscreenModeForTab returns false - PiP windows cannot go
// fullscreen.
TEST_F(DocumentPipHostTest, CanEnterFullscreenModeForTab_ReturnsFalse) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_FALSE(
      host->CanEnterFullscreenModeForTab(/*requesting_frame=*/nullptr));
}

// GetCanResize returns true - PiP windows are resizable.
TEST_F(DocumentPipHostTest, GetCanResize_ReturnsTrue) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_TRUE(host->GetCanResize());
}

// GetWindowShowState delegates to the widget and returns a valid state.
TEST_F(DocumentPipHostTest, GetWindowShowState_ReturnsNormalState) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  // The widget is not minimized/maximized/fullscreen after creation.
  ui::mojom::WindowShowState state = host->GetWindowShowState();
  EXPECT_EQ(ui::mojom::WindowShowState::kNormal, state);
}

// GetWindowShowState returns kDefault when there is no widget.
TEST_F(DocumentPipHostTest, GetWindowShowState_NoWidget_ReturnsDefault) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);
  host->CloseContents(host->GetChildWebContents());

  EXPECT_EQ(ui::mojom::WindowShowState::kDefault, host->GetWindowShowState());
}

// CanOverscrollContent returns false.
TEST_F(DocumentPipHostTest, CanOverscrollContent_ReturnsFalse) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_FALSE(host->CanOverscrollContent());
}

// IsBackForwardCacheSupported returns true.
TEST_F(DocumentPipHostTest, IsBackForwardCacheSupported_ReturnsTrue) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_TRUE(host->IsBackForwardCacheSupported(*host->GetChildWebContents()));
}

// ShouldFocusLocationBarByDefault returns false - PiP has no location bar.
TEST_F(DocumentPipHostTest, ShouldFocusLocationBarByDefault_ReturnsFalse) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_FALSE(
      host->ShouldFocusLocationBarByDefault(host->GetChildWebContents()));
}

// ShouldUseInstancedSystemMediaControls returns false.
TEST_F(DocumentPipHostTest,
       ShouldUseInstancedSystemMediaControls_ReturnsFalse) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_FALSE(host->ShouldUseInstancedSystemMediaControls());
}

// GetResponsibleWebContents returns the PiP WebContents itself.
TEST_F(DocumentPipHostTest, GetResponsibleWebContents_ReturnsPipContents) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  content::WebContents* child = host->GetChildWebContents();
  EXPECT_EQ(child, host->GetResponsibleWebContents(child));
}

// GetTitleForMediaControls returns an empty string.
TEST_F(DocumentPipHostTest, GetTitleForMediaControls_ReturnsEmpty) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_TRUE(
      host->GetTitleForMediaControls(host->GetChildWebContents()).empty());
}

// DidAddMessageToConsole returns false - does not consume the message.
TEST_F(DocumentPipHostTest, DidAddMessageToConsole_DoesNotConsume) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_FALSE(host->DidAddMessageToConsole(
      host->GetChildWebContents(), blink::mojom::ConsoleMessageLevel::kInfo,
      u"test message",
      /*line_no=*/1, u"test_source"));
}

// GetJavaScriptDialogManager returns nullptr until future CL wires the dialog
// delegate.
TEST_F(DocumentPipHostTest, GetJavaScriptDialogManager_ReturnsNullptr) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_EQ(nullptr,
            host->GetJavaScriptDialogManager(host->GetChildWebContents()));
}

// IsContentsActive always returns true for PiP.
TEST_F(DocumentPipHostTest, IsContentsActive_ReturnsTrue) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_TRUE(host->IsContentsActive(host->GetChildWebContents()));
}

// GetWindowBoundsInScreen returns the widget bounds when widget exists.
TEST_F(DocumentPipHostTest, GetWindowBoundsInScreen_ReturnsWidgetBounds) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  std::optional<gfx::Rect> bounds = host->GetWindowBoundsInScreen();
  EXPECT_TRUE(bounds.has_value());
}

// GetWindowBoundsInScreen returns nullopt when there is no widget.
TEST_F(DocumentPipHostTest, GetWindowBoundsInScreen_NoWidget_ReturnsNullopt) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);
  host->CloseContents(host->GetChildWebContents());

  EXPECT_FALSE(host->GetWindowBoundsInScreen().has_value());
}

// BeforeUnloadFired always proceeds - PiP has no beforeunload interstitial.
TEST_F(DocumentPipHostTest, BeforeUnloadFired_AlwaysProceeds) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  bool proceed_to_fire_unload = false;
  host->BeforeUnloadFired(host->GetChildWebContents(),
                          /*proceed=*/true, &proceed_to_fire_unload);
  EXPECT_TRUE(proceed_to_fire_unload);
}

// TakeFocus returns false - PiP has a single content area.
TEST_F(DocumentPipHostTest, TakeFocus_ReturnsFalse) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_FALSE(host->TakeFocus(host->GetChildWebContents(), /*reverse=*/false));
  EXPECT_FALSE(host->TakeFocus(host->GetChildWebContents(), /*reverse=*/true));
}

// HandleKeyboardEvent returns false - no browser chrome accelerators.
TEST_F(DocumentPipHostTest, HandleKeyboardEvent_ReturnsFalse) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  input::NativeWebKeyboardEvent event(blink::WebInputEvent::Type::kRawKeyDown,
                                      blink::WebInputEvent::kNoModifiers,
                                      base::TimeTicks::Now());
  EXPECT_FALSE(host->HandleKeyboardEvent(host->GetChildWebContents(), event));
}

// PreHandleKeyboardEvent returns NOT_HANDLED.
TEST_F(DocumentPipHostTest, PreHandleKeyboardEvent_ReturnsNotHandled) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  input::NativeWebKeyboardEvent event(blink::WebInputEvent::Type::kRawKeyDown,
                                      blink::WebInputEvent::kNoModifiers,
                                      base::TimeTicks::Now());
  EXPECT_EQ(content::KeyboardEventProcessingResult::NOT_HANDLED,
            host->PreHandleKeyboardEvent(host->GetChildWebContents(), event));
}

// AddNewContents forwards the popup to the opener's delegate (the success
// path) and leaves it unblocked.
TEST_F(DocumentPipHostTest, AddNewContents_ForwardsToOpenerDelegate) {
  RecordingAddNewContentsDelegate opener_delegate;
  opener()->SetDelegate(&opener_delegate);
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  bool was_blocked = false;
  auto popup =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  content::WebContents* popup_ptr = popup.get();
  content::WebContents* result = host->AddNewContents(
      host->GetChildWebContents(), std::move(popup), GURL(),
      WindowOpenDisposition::NEW_POPUP, blink::mojom::WindowFeatures(),
      /*user_gesture=*/true, &was_blocked);

  EXPECT_EQ(1, opener_delegate.call_count());
  EXPECT_EQ(opener(), opener_delegate.source());
  EXPECT_EQ(WindowOpenDisposition::NEW_POPUP, opener_delegate.disposition());
  EXPECT_EQ(popup_ptr, result);
  EXPECT_FALSE(was_blocked);
  opener()->SetDelegate(nullptr);
}

// SetContentsBounds resizes the PiP widget to the requested bounds.
TEST_F(DocumentPipHostTest, SetContentsBounds_ResizesWidget) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  const gfx::Rect new_bounds(100, 120, 500, 400);
  host->SetContentsBounds(host->GetChildWebContents(), new_bounds);

  std::optional<gfx::Rect> bounds = host->GetWindowBoundsInScreen();
  ASSERT_TRUE(bounds.has_value());
  EXPECT_EQ(new_bounds.size(), bounds->size());
}
