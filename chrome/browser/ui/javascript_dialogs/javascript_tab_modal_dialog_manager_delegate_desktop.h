// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_DESKTOP_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager_delegate.h"

class JavaScriptTabModalDialogManagerDelegateDesktop
    : public javascript_dialogs::TabModalDialogManagerDelegate,
      public BrowserListObserver,
      public TabStripModelObserver {
 public:
  explicit JavaScriptTabModalDialogManagerDelegateDesktop(
      content::WebContents* web_contents);
  ~JavaScriptTabModalDialogManagerDelegateDesktop() override;

  // javascript_dialogs::TabModalDialogManagerDelegate
  base::WeakPtr<javascript_dialogs::TabModalDialogView> CreateNewDialog(
      content::WebContents* alerting_web_contents,
      const base::string16& title,
      content::JavaScriptDialogType dialog_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_closed_callback) override;
  void WillRunDialog() override;
  void DidCloseDialog() override;
  void SetTabNeedsAttention(bool attention) override;
  bool IsWebContentsForemost() override;
  bool IsApp() override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  // Marks the tab as needing attention.
  void SetTabNeedsAttentionImpl(bool attention,
                                TabStripModel* tab_strip_model,
                                int index);

  // If this instance is observing a TabStripModel, then this member is not
  // nullptr.
  //
  // This raw pointer is safe to use because the lifetime of this instance is
  // tied to the corresponding WebContents, which is owned by the TabStripModel.
  // Any time TabStripModel would give up ownership of the WebContents, it would
  // first send either a TabStripModelChange::kRemoved or kReplaced
  // via OnTabStripModelChanged() callback, which gives this instance the
  // opportunity to stop observing the TabStripModel.
  //
  // A TabStripModel cannot be destroyed without first detaching all of its
  // WebContents.
  TabStripModel* tab_strip_model_being_observed_ = nullptr;

  // The WebContents for the tab over which the dialog will be modal. This may
  // be different from the WebContents that requested the dialog, such as with
  // Chrome app <webview>s.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptTabModalDialogManagerDelegateDesktop);
};

#endif  // CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_DESKTOP_H_
