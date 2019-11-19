// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_native_app_modal_dialog_factory.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/app_modal/javascript_native_dialog_factory.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

#if defined(USE_X11)
#include "chrome/browser/ui/views/javascript_app_modal_dialog_views_x11.h"
#else
#include "chrome/browser/ui/blocked_content/popunder_preventer.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/views/javascript_app_modal_dialog_views.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

#if !defined(USE_X11)
class ChromeJavaScriptAppModalDialogViews
    : public app_modal::JavaScriptAppModalDialogViews {
 public:
  explicit ChromeJavaScriptAppModalDialogViews(
      app_modal::JavaScriptAppModalDialog* parent)
      : app_modal::JavaScriptAppModalDialogViews(parent),
        popunder_preventer_(new PopunderPreventer(parent->web_contents())) {}
  ~ChromeJavaScriptAppModalDialogViews() override {}

 private:
  std::unique_ptr<PopunderPreventer> popunder_preventer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeJavaScriptAppModalDialogViews);
};
#endif

class ChromeJavaScriptNativeDialogViewsFactory
    : public app_modal::JavaScriptNativeDialogFactory {
 public:
  ChromeJavaScriptNativeDialogViewsFactory() {}
  ~ChromeJavaScriptNativeDialogViewsFactory() override {}

 private:
  app_modal::NativeAppModalDialog* CreateNativeJavaScriptDialog(
      app_modal::JavaScriptAppModalDialog* dialog) override {
    app_modal::JavaScriptAppModalDialogViews* d = nullptr;
#if defined(USE_X11)
    d = new JavaScriptAppModalDialogViewsX11(dialog);
#else
    d = new ChromeJavaScriptAppModalDialogViews(dialog);
#endif

    dialog->web_contents()->GetDelegate()->ActivateContents(
        dialog->web_contents());
    gfx::NativeWindow parent_window =
        dialog->web_contents()->GetTopLevelNativeWindow();
#if defined(USE_AURA)
    if (!parent_window->GetRootWindow()) {
      // When we are part of a WebContents that isn't actually being displayed
      // on the screen, we can't actually attach to it.
      parent_window = NULL;
    }
#endif
    constrained_window::CreateBrowserModalDialogViews(d, parent_window);
    return d;
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeJavaScriptNativeDialogViewsFactory);
};

}  // namespace

void InstallChromeJavaScriptNativeAppModalDialogFactory() {
  app_modal::JavaScriptDialogManager::GetInstance()->SetNativeDialogFactory(
      base::WrapUnique(new ChromeJavaScriptNativeDialogViewsFactory));
}
