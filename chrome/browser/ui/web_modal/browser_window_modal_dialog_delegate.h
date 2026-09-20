// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_MODAL_BROWSER_WINDOW_MODAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_WEB_MODAL_BROWSER_WINDOW_MODAL_DIALOG_DELEGATE_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

// Serves as the WebContentsModalDialogManagerDelegate for each tab's
// WebContentsModalDialogManager. Handles dialog host lookup, tab blocking,
// and fullscreen exit when modal dialogs are shown.
// Delegate wiring (SetDelegate) is managed by Browser::SetAsDelegate to
// ensure correct timing relative to layout during tab insertion.
class BrowserWindowModalDialogDelegate
    : public ChromeWebModalDialogManagerDelegate,
      public TabStripModelObserver {
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

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void NotifyModalDialogsPositionRequiresUpdate();

 private:
  void DropFullscreenForSecurity(
      base::WeakPtr<content::WebContents> web_contents);

  const raw_ptr<BrowserWindowInterface> browser_;

  // Per-WebContents fullscreen blocks held while a tab-modal dialog is showing.
  // See WebContents::ForSecurityDropFullscreen().
  base::flat_map<content::WebContents*, base::ScopedClosureRunner>
      fullscreen_blocks_;

  ui::ScopedUnownedUserData<BrowserWindowModalDialogDelegate>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<BrowserWindowModalDialogDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEB_MODAL_BROWSER_WINDOW_MODAL_DIALOG_DELEGATE_H_
