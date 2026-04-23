// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_MODAL_BROWSER_WINDOW_MODAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_WEB_MODAL_BROWSER_WINDOW_MODAL_DIALOG_DELEGATE_H_

#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// Serves as the WebContentsModalDialogManagerDelegate for each tab's
// WebContentsModalDialogManager. Handles dialog host lookup, tab blocking,
// and fullscreen exit when modal dialogs are shown.
// Delegate wiring (SetDelegate) is managed by Browser::SetAsDelegate to
// ensure correct timing relative to layout during tab insertion.
class BrowserWindowModalDialogDelegate
    : public ChromeWebModalDialogManagerDelegate {
 public:
  DECLARE_USER_DATA(BrowserWindowModalDialogDelegate);

  explicit BrowserWindowModalDialogDelegate(BrowserWindowInterface* browser);
  BrowserWindowModalDialogDelegate(const BrowserWindowModalDialogDelegate&) =
      delete;
  BrowserWindowModalDialogDelegate& operator=(
      const BrowserWindowModalDialogDelegate&) = delete;
  ~BrowserWindowModalDialogDelegate() override;

  static BrowserWindowModalDialogDelegate* From(
      BrowserWindowInterface* browser);

  // ChromeWebModalDialogManagerDelegate:
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
  ui::ScopedUnownedUserData<BrowserWindowModalDialogDelegate>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_WEB_MODAL_BROWSER_WINDOW_MODAL_DIALOG_DELEGATE_H_
