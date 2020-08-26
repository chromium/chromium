// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_app_modal_dialog_view_factory.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/blocked_content/popunder_preventer.h"
#include "chrome/browser/ui/views/javascript_app_modal_event_blocker.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/javascript_dialogs/views/app_modal_dialog_view_views.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

bool UseEventBlocker() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    return ui::OzonePlatform::GetInstance()
        ->GetPlatformProperties()
        .app_modal_dialogs_use_event_blocker;
  }
#endif
#if defined(USE_X11)
  return true;
#else
  return false;
#endif
}

class ChromeJavaScriptAppModalDialogViews
    : public javascript_dialogs::AppModalDialogViewViews {
 public:
  explicit ChromeJavaScriptAppModalDialogViews(
      javascript_dialogs::AppModalDialogController* parent)
      : javascript_dialogs::AppModalDialogViewViews(parent),
        popunder_preventer_(parent->web_contents()) {}
  ~ChromeJavaScriptAppModalDialogViews() override = default;

  // JavaScriptAppModalDialogViews:
  void ShowAppModalDialog() override {
    // BrowserView::CanActivate() ensures that other browser windows cannot be
    // activated while the dialog is visible.  Block events to other browser
    // windows so that the user cannot interact with them.  This hack is
    // unnecessary on Windows and Chrome OS.
    // TODO(pkotwicz): Find a better way of doing this and remove this hack.
    if (UseEventBlocker() && !event_blocker_.get()) {
      event_blocker_ = std::make_unique<JavascriptAppModalEventBlocker>(
          GetWidget()->GetNativeView());
    }
    AppModalDialogViewViews::ShowAppModalDialog();
  }

  // views::DialogDelegate:
  void WindowClosing() override { event_blocker_.reset(); }

 private:
  // Blocks events to other browser windows while the dialog is open.
  std::unique_ptr<JavascriptAppModalEventBlocker> event_blocker_;

  PopunderPreventer popunder_preventer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeJavaScriptAppModalDialogViews);
};

javascript_dialogs::AppModalDialogView* CreateNativeJavaScriptDialog(
    javascript_dialogs::AppModalDialogController* dialog) {
  javascript_dialogs::AppModalDialogViewViews* d =
      new ChromeJavaScriptAppModalDialogViews(dialog);
  dialog->web_contents()->GetDelegate()->ActivateContents(
      dialog->web_contents());
  gfx::NativeWindow parent_window =
      dialog->web_contents()->GetTopLevelNativeWindow();
#if defined(USE_AURA)
  if (!parent_window->GetRootWindow()) {
    // When we are part of a WebContents that isn't actually being displayed
    // on the screen, we can't actually attach to it.
    parent_window = nullptr;
  }
#endif
  constrained_window::CreateBrowserModalDialogViews(d, parent_window);
  return d;
}

}  // namespace

void InstallChromeJavaScriptAppModalDialogViewFactory() {
  javascript_dialogs::AppModalDialogManager::GetInstance()
      ->SetNativeDialogFactory(
          base::BindRepeating(&CreateNativeJavaScriptDialog));
}
