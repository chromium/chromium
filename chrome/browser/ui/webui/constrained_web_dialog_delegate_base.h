// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_
#define CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace content {
class BrowserContext;
}

namespace ui {
class WebDialogDelegate;
}

// Platform-agnostic base implementation of ConstrainedWebDialogDelegate.
class ConstrainedWebDialogDelegateBase
    : public ConstrainedWebDialogDelegate,
      public content::WebContentsObserver,
      public ui::WebDialogWebContentsDelegate {
 public:
  // |browser_context| and |delegate| must outlive |this| instance, whereas
  // |this| will take ownership of |tab_delegate|.
  ConstrainedWebDialogDelegateBase(content::BrowserContext* browser_context,
                                   ui::WebDialogDelegate* delegate,
                                   WebDialogWebContentsDelegate* tab_delegate);
  ~ConstrainedWebDialogDelegateBase() override;

  bool closed_via_webui() const;

  // ConstrainedWebDialogDelegate interface.
  const ui::WebDialogDelegate* GetWebDialogDelegate() const override;
  ui::WebDialogDelegate* GetWebDialogDelegate() override;
  void OnDialogCloseFromWebUI() override;
  std::unique_ptr<content::WebContents> ReleaseWebContents() override;
  content::WebContents* GetWebContents() override;
  gfx::NativeWindow GetNativeDialog() override;
  gfx::Size GetConstrainedWebDialogMinimumSize() const override;
  gfx::Size GetConstrainedWebDialogMaximumSize() const override;
  gfx::Size GetConstrainedWebDialogPreferredSize() const override;

  // WebContentsObserver interface
  void WebContentsDestroyed() override;

  // WebDialogWebContentsDelegate interface.
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  // Resize the dialog to the given size.
  virtual void ResizeToGivenSize(const gfx::Size size);

 private:
  std::unique_ptr<ui::WebDialogDelegate> web_dialog_delegate_;

  // Holds the HTML to display in the constrained dialog.
  std::unique_ptr<content::WebContents> web_contents_holder_;

  // Pointer to the WebContents in |web_contents_holder_| for the lifetime of
  // that object, even if ReleaseWebContents() gets called. If the WebContents
  // gets destroyed, |web_contents_| will be set to a nullptr.
  content::WebContents* web_contents_;

  // Was the dialog closed from WebUI (in which case |web_dialog_delegate_|'s
  // OnDialogClosed() method has already been called)?
  bool closed_via_webui_;

  std::unique_ptr<WebDialogWebContentsDelegate> override_tab_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateBase);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_
