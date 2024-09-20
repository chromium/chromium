// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_WEB_DIALOG_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_WEB_DIALOG_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace ash {

// Launches web dialog during OOBE/Login with specified URL and title.
class LoginWebDialog : public ui::WebDialogDelegate {
 public:
  // If `parent_window` is null then the dialog is placed in the modal dialog
  // container on the primary display.
  LoginWebDialog(content::BrowserContext* browser_context,
                 gfx::NativeWindow parent_window,
                 const std::u16string& title,
                 const GURL& url);

  LoginWebDialog(const LoginWebDialog&) = delete;
  LoginWebDialog& operator=(const LoginWebDialog&) = delete;

  ~LoginWebDialog() override;

  void Show();

  static content::WebContents* GetCurrentWebContents();

  // Returns `dialog_window_` instance for test, can be NULL if dialog is not
  // shown or closed.
  gfx::NativeWindow get_dialog_window_for_test() const {
    return dialog_window_;
  }

 protected:
  // ui::WebDialogDelegate implementation.
  void GetDialogSize(gfx::Size* size) const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool HandleOpenURLFromTab(content::WebContents* source,
                            const content::OpenURLParams& params,
                            base::OnceCallback<void(content::NavigationHandle&)>
                                navigation_handle_callback,
                            content::WebContents** out_new_contents) override;

 private:
  bool MaybeCloseWindow(ui::WebDialogDelegate& delegate,
                        const ui::Accelerator& accelerator);
  void OnDialogClosing(const std::string& json_retval);

  const raw_ptr<content::BrowserContext> browser_context_;
  gfx::NativeWindow parent_window_;
  gfx::NativeWindow dialog_window_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_WEB_DIALOG_H_
