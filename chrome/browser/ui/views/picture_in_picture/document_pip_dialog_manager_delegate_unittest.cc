// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_dialog_manager_delegate.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_view.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/non_client_view.h"

namespace {

blink::mojom::PictureInPictureWindowOptions MakeDefaultPipOptions() {
  blink::mojom::PictureInPictureWindowOptions opts;
  opts.width = 400;
  opts.height = 300;
  opts.disallow_return_to_opener = false;
  opts.prefer_initial_window_placement = false;
  return opts;
}

gfx::Rect MakeDefaultInitialBounds() {
  return gfx::Rect(20, 30, 400, 300);
}

// Supplies a default context for top-level widgets created without a parent or
// context. In production the Document PiP widget is a DesktopNativeWidgetAura,
// which needs neither; these unit tests use in-process NativeWidgetAura (so
// window-modal JS dialogs work in the headless unit_tests environment), and
// NativeWidgetAura DCHECKs that a parent or context is set. Without this,
// DocumentPipHost::CreateAndShowPipWindow() crashes during Widget::Init().
class ContextSupplyingViewsDelegate : public views::TestViewsDelegate {
 public:
  void set_default_context(gfx::NativeWindow context) { context_ = context; }

  // views::TestViewsDelegate:
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override {
    if (!params->parent && !params->context) {
      params->context = context_;
    }
    views::TestViewsDelegate::OnBeforeWidgetInit(params, delegate);
  }

 private:
  gfx::NativeWindow context_;
};

}  // namespace

class DocumentPipDialogManagerDelegateTest : public ChromeViewsTestBase {
 public:
  DocumentPipDialogManagerDelegateTest() = default;

  void SetUp() override {
    // Install a ViewsDelegate that gives context-less top-level widgets (like
    // the Document PiP widget, created by production code) the test root window
    // as their context. This lets the PiP widget use in-process
    // NativeWidgetAura, which -- unlike DesktopNativeWidgetAura -- supports
    // window-modal child dialogs in the headless unit_tests environment:
    // NativeWidgetAura::InitModalType just sets an aura property, whereas
    // DesktopWindowTreeHostLinux::InitModalType is NOTIMPLEMENTED.
    ContextSupplyingViewsDelegate* context_delegate =
        set_views_delegate(std::make_unique<ContextSupplyingViewsDelegate>());

    ChromeViewsTestBase::SetUp();

    // GetContext() only returns a valid root window after the test helper is
    // created by ChromeViewsTestBase::SetUp() above. The delegate is owned by
    // the test helper and outlives this call.
    context_delegate->set_default_context(GetContext());

    opener_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    ASSERT_TRUE(opener_web_contents_);

    // DocumentPipFrameView reads the opener's SecurityStateTabHelper to render
    // the origin chip security icon and CHECK()s the invariant, so attach it
    // here as TabHelpers would in production.
    ChromeSecurityStateTabHelper::CreateForWebContents(
        opener_web_contents_.get());

    // Realize the opener WebContents in a top-level widget, as it would be
    // hosted in a browser window in production.
    opener_host_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto* web_view = opener_host_widget_->SetContentsView(
        std::make_unique<views::WebView>(&profile_));
    web_view->SetWebContents(opener_web_contents_.get());
    opener_host_widget_->Show();
  }

  void TearDown() override {
    opener_web_contents_.reset();
    opener_host_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  // Creates a DocumentPipHost attached to the opener with a PiP widget open and
  // a child WebContents whose main frame has committed a navigation (so dialog
  // titles can be computed from its origin).
  DocumentPipHost* CreateHostAndOpenPipWindow(
      const gfx::Rect& initial_bounds = MakeDefaultInitialBounds()) {
    DocumentPipHost::CreateForWebContents(opener());
    auto* host = DocumentPipHost::FromWebContents(opener());
    auto child =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    content::WebContentsTester::For(child.get())
        ->NavigateAndCommit(GURL("https://pip.example/"));
    // Seed the PictureInPictureWindowManager's opener display, which production
    // sets via CalculateInitialPictureInPictureWindowBounds() before the host
    // is created (see
    // PictureInPictureWindowManager::EnterDocumentPictureInPicture).
    // DocumentPipFrameView::UpdateWindowBoundsForRequestedInnerSize(), run
    // during CreateAndShowPipWindow() below, CHECK()s that it is set.
    PictureInPictureWindowManager::GetInstance()
        ->CalculateInitialPictureInPictureWindowBounds(
            MakeDefaultPipOptions(),
            display::Screen::Get()->GetPrimaryDisplay());
    host->CreateAndShowPipWindow(std::move(child), MakeDefaultPipOptions(),
                                 initial_bounds);
    return host;
  }

  content::WebContents* opener() { return opener_web_contents_.get(); }

 protected:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<views::Widget> opener_host_widget_;
  std::unique_ptr<content::WebContents> opener_web_contents_;
};

// The host wires up a TabModalDialogManager on the child WebContents so that
// GetJavaScriptDialogManager() returns a non-null manager.
TEST_F(DocumentPipDialogManagerDelegateTest,
       GetJavaScriptDialogManager_AttachedToChild) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  content::WebContents* child = host->GetChildWebContents();
  ASSERT_TRUE(child);

  ASSERT_EQ(host, child->GetDelegate());

  content::JavaScriptDialogManager* manager =
      child->GetDelegate()->GetJavaScriptDialogManager(child);
  ASSERT_TRUE(manager);
  EXPECT_EQ(javascript_dialogs::TabModalDialogManager::FromWebContents(child),
            manager);
}

// The standalone PiP window is always its own exclusive modal context.
TEST_F(DocumentPipDialogManagerDelegateTest, CanShowModalUI_True) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  DocumentPipDialogManagerDelegate delegate(host->GetWidget());
  EXPECT_TRUE(delegate.CanShowModalUI());
}

// The standalone PiP window is foreground for JavaScript dialog purposes even
// when native widget activation reports false.
TEST_F(DocumentPipDialogManagerDelegateTest, IsWebContentsForemost_True) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  host->GetWidget()->Deactivate();

  DocumentPipDialogManagerDelegate delegate(host->GetWidget());
  EXPECT_TRUE(delegate.IsWebContentsForemost());
}

// Regression test: confirm()/prompt() are suppressed by TabModalDialogManager
// if the delegate reports the WebContents as not foremost.
TEST_F(DocumentPipDialogManagerDelegateTest,
       ManagerConfirmFromInactiveWidget_ShowsDialog) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  content::WebContents* child = host->GetChildWebContents();
  ASSERT_TRUE(child);
  host->GetWidget()->Deactivate();

  auto* manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(child);
  ASSERT_TRUE(manager);

  bool did_suppress_message = true;
  bool called = false;
  bool success = true;
  manager->RunJavaScriptDialog(
      child, child->GetPrimaryMainFrame(),
      content::JAVASCRIPT_DIALOG_TYPE_CONFIRM, u"ok?",
      /*default_prompt_text=*/std::u16string(),
      base::BindLambdaForTesting([&](bool s, const std::u16string& i) {
        called = true;
        success = s;
      }),
      &did_suppress_message);

  EXPECT_FALSE(did_suppress_message);
  EXPECT_TRUE(manager->IsShowingDialogForTesting());

  manager->ClickDialogButtonForTesting(/*accept=*/false, std::u16string());

  EXPECT_TRUE(called);
  EXPECT_FALSE(success);
}

// The standalone PiP window is not a PWA/app window.
TEST_F(DocumentPipDialogManagerDelegateTest, IsApp_False) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  DocumentPipDialogManagerDelegate delegate(host->GetWidget());
  EXPECT_FALSE(delegate.IsApp());
}

// alert(): a window-modal dialog is parented to the PiP widget, and accepting
// it runs the dialog-closed callback with success=true.
TEST_F(DocumentPipDialogManagerDelegateTest, Alert_AcceptRunsCallback) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  DocumentPipDialogManagerDelegate delegate(host->GetWidget());

  bool called = false;
  bool success = false;
  std::u16string input;
  base::WeakPtr<javascript_dialogs::TabModalDialogView> view =
      delegate.CreateNewDialog(
          host->GetChildWebContents(), u"example says", /*dialog_type=*/
          content::JAVASCRIPT_DIALOG_TYPE_ALERT, u"hello",
          /*default_prompt_text=*/std::u16string(),
          base::BindLambdaForTesting([&](bool s, const std::u16string& i) {
            called = true;
            success = s;
            input = i;
          }),
          base::DoNothing());
  ASSERT_TRUE(view);

  views::Widget* dialog_widget = delegate.GetActiveDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  dialog_widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();

  EXPECT_TRUE(called);
  EXPECT_TRUE(success);
  EXPECT_EQ(std::u16string(), input);
}

// confirm(): cancelling the dialog runs the callback with success=false.
TEST_F(DocumentPipDialogManagerDelegateTest, Confirm_CancelRunsCallback) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  DocumentPipDialogManagerDelegate delegate(host->GetWidget());

  bool called = false;
  bool success = true;
  base::WeakPtr<javascript_dialogs::TabModalDialogView> view =
      delegate.CreateNewDialog(
          host->GetChildWebContents(), u"example says",
          content::JAVASCRIPT_DIALOG_TYPE_CONFIRM, u"ok?",
          /*default_prompt_text=*/std::u16string(),
          base::BindLambdaForTesting([&](bool s, const std::u16string& i) {
            called = true;
            success = s;
          }),
          base::DoNothing());
  ASSERT_TRUE(view);

  views::Widget* dialog_widget = delegate.GetActiveDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  dialog_widget->widget_delegate()->AsDialogDelegate()->CancelDialog();

  EXPECT_TRUE(called);
  EXPECT_FALSE(success);
}

// prompt(): accepting returns the prompt field's text.
TEST_F(DocumentPipDialogManagerDelegateTest, Prompt_ReturnsInput) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  DocumentPipDialogManagerDelegate delegate(host->GetWidget());

  bool success = false;
  std::u16string input;
  base::WeakPtr<javascript_dialogs::TabModalDialogView> view =
      delegate.CreateNewDialog(
          host->GetChildWebContents(), u"example says",
          content::JAVASCRIPT_DIALOG_TYPE_PROMPT, u"enter:",
          /*default_prompt_text=*/u"default-input",
          base::BindLambdaForTesting([&](bool s, const std::u16string& i) {
            success = s;
            input = i;
          }),
          base::DoNothing());
  ASSERT_TRUE(view);
  EXPECT_EQ(u"default-input", view->GetUserInput());

  views::Widget* dialog_widget = delegate.GetActiveDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  dialog_widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();

  EXPECT_TRUE(success);
  EXPECT_EQ(u"default-input", input);
}

// Opening a dialog larger than the PiP window grows the PiP window to contain
// it; closing the dialog restores the PiP window to its pre-dialog bounds.
TEST_F(DocumentPipDialogManagerDelegateTest,
       ResizesPipToContainDialogThenRestores) {
  // Open a PiP window small enough that any dialog exceeds it, so growth is
  // guaranteed regardless of platform dialog metrics.
  DocumentPipHost* host =
      CreateHostAndOpenPipWindow(gfx::Rect(20, 30, 120, 80));
  const gfx::Rect original_bounds =
      host->GetWidget()->GetWindowBoundsInScreen();

  DocumentPipDialogManagerDelegate delegate(host->GetWidget());
  base::WeakPtr<javascript_dialogs::TabModalDialogView> view =
      delegate.CreateNewDialog(
          host->GetChildWebContents(), u"example says",
          content::JAVASCRIPT_DIALOG_TYPE_ALERT,
          u"a fairly long alert message that needs more room than the window",
          /*default_prompt_text=*/std::u16string(), base::DoNothing(),
          base::DoNothing());
  ASSERT_TRUE(view);

  views::Widget* dialog_widget = delegate.GetActiveDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);
  EXPECT_FALSE(host->IsChildResizePendingForTesting());

  // The PiP window grew to contain the dialog.
  const gfx::Rect grown_bounds = host->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_GE(grown_bounds.width(), original_bounds.width());
  EXPECT_GE(grown_bounds.height(), original_bounds.height());
  EXPECT_NE(grown_bounds.size(), original_bounds.size());

  // Closing the dialog restores the PiP window to its pre-dialog size. The
  // production restore path intentionally preserves the current origin, since
  // the user may have moved the window while the dialog was open.
  views::test::WidgetDestroyedWaiter dialog_waiter(dialog_widget);
  dialog_widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  dialog_waiter.Wait();
  EXPECT_EQ(original_bounds.size(),
            host->GetWidget()->GetWindowBoundsInScreen().size());
}

// Tearing down the PiP window while a dialog is open closes the dialog cleanly
// (no crash) and runs the force-closed callback rather than the user callback.
TEST_F(DocumentPipDialogManagerDelegateTest, WidgetClosedMidDialog_NoCrash) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();

  bool user_callback_ran = false;
  bool force_closed_ran = false;
  // Scope the delegate so it is destroyed before host->Close() tears down the
  // widget it references. The dialog view itself is owned by its own widget
  // (a child of the PiP widget), so it survives the delegate going away.
  {
    DocumentPipDialogManagerDelegate delegate(host->GetWidget());
    base::WeakPtr<javascript_dialogs::TabModalDialogView> view =
        delegate.CreateNewDialog(
            host->GetChildWebContents(), u"example says",
            content::JAVASCRIPT_DIALOG_TYPE_ALERT, u"hello",
            /*default_prompt_text=*/std::u16string(),
            base::BindLambdaForTesting(
                [&](bool, const std::u16string&) { user_callback_ran = true; }),
            base::BindLambdaForTesting([&]() { force_closed_ran = true; }));
    ASSERT_TRUE(view);
    ASSERT_TRUE(delegate.GetActiveDialogWidgetForTesting());
  }

  // Tear down the PiP window; the child dialog widget should close with it,
  // which runs the force-closed callback.
  host->Close();
  EXPECT_TRUE(base::test::RunUntil([&]() { return force_closed_ran; }));

  EXPECT_FALSE(user_callback_ran);
  EXPECT_TRUE(force_closed_ran);
}
