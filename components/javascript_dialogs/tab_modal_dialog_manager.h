// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_TAB_MODAL_DIALOG_MANAGER_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_TAB_MODAL_DIALOG_MANAGER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager_delegate.h"
#include "components/javascript_dialogs/tab_modal_dialog_view.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace javascript_dialogs {

// A class that serves as the JavaScriptDialogManager for tab modal JavaScript
// dialogs.
//
// This implements two different functionalities for JavaScript dialogs.
//
// window.alert() dialogs are tab-modal dialogs. If a tab calls alert() while it
// is foremost, a dialog is displayed and the renderer is held blocked. When the
// user switches to a different tab, or if the dialog is shown while the tab is
// not foremost, while the dialog is shown, the renderer is not held blocked.
//
// window.confirm() and window.prompt() dialogs are auto-dismissing,
// dialogs that close when the user switches away to a different tab. Because
// JavaScript dialogs are synchronous and block arbitrary sets of renderers,
// they cannot be made tab-modal. Therefore the next best option is to make them
// auto-closing, so that they never block the user's access to other renderers.
//
// References:
//   http://bit.ly/project-oldspice
class TabModalDialogManager
    : public content::JavaScriptDialogManager,
      public content::WebContentsObserver,
      public content::WebContentsUserData<TabModalDialogManager> {
 public:
  enum class DismissalCause {
    // This is used for a UMA histogram. Please never alter existing values,
    // only append new ones.

    // The tab helper is destroyed. By current design, the dialog is always torn
    // down before the tab helper is destroyed, so we never see the
    // |kTabHelperDestroyed| enum. However, that might not always be the case.
    // It's better to have a simple rule in the code of "when you close a dialog
    // you must provide a UMA enum reason why" and have some enums that never
    // happen than have haphazard code that enforces no rules.
    kTabHelperDestroyed = 0,

    // Subsequent dialog pops up.
    kSubsequentDialogShown = 1,

    // HandleJavaScriptDialog() is called. In practice, this can happen whenever
    // browser choose to accept/cancel the dialog without user's interaction.
    kHandleDialogCalled = 2,

    // CancelDialogs() is called. In practice, this can happen whenever browser
    // choose to close the dialog without user's interaction. Besides, this can
    // also happen when tab is closed by user on a Mac platform.
    kCancelDialogsCalled = 3,

    // Tab is made hidden by opening a new tab, switching to another tab, etc.
    // Note that only Prompt() and Confirm() can be dismissed for this cause;
    // it won't affect Alert().
    kTabHidden = 4,

    // Another browser instance is made active.
    kBrowserSwitched = 5,

    // Accept/Cancel button is clicked by user.
    kDialogButtonClicked = 6,

    // Navigation occurs.
    kTabNavigated = 7,

    // Tab's contents was replaced.
    kTabSwitchedOut = 8,

    // CloseDialog() is called. In practice, this happens when tab is closed by
    // user on a non-Mac platform.
    kDialogClosed = 9,

    kMaxValue = kDialogClosed,
  };

  TabModalDialogManager(const TabModalDialogManager&) = delete;
  TabModalDialogManager& operator=(const TabModalDialogManager&) = delete;

  ~TabModalDialogManager() override;

  void BrowserActiveStateChanged();
  void CloseDialogWithReason(DismissalCause reason);

  void SetDialogShownCallbackForTesting(base::OnceClosure callback);
  bool IsShowingDialogForTesting() const;
  void ClickDialogButtonForTesting(bool accept,
                                   const std::u16string& user_input);
  using DialogDismissedCallback = base::OnceCallback<void(DismissalCause)>;
  void SetDialogDismissedCallbackForTesting(DialogDismissedCallback callback);

  // JavaScriptDialogManager:
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           content::RenderFrameHost* render_frame_host,
                           content::JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             content::RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const std::u16string* prompt_override) override;
  void CancelDialogs(content::WebContents* web_contents,
                     bool reset_state) override;

  // WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<TabModalDialogManager>;

  TabModalDialogManager(
      content::WebContents* web_contents,
      std::unique_ptr<TabModalDialogManagerDelegate> delegate);

  // Logs the cause of a dialog dismissal in UMA.
  void LogDialogDismissalCause(DismissalCause cause);

  // Handles the case when the user switches away from a tab.
  void HandleTabSwitchAway(DismissalCause cause);

  // This closes any open dialog. It is safe to call if there is no currently
  // open dialog.
  void CloseDialog(DismissalCause cause,
                   bool success,
                   const std::u16string& user_input);

  // There can be at most one dialog (pending or not) being shown at any given
  // time on a tab. Depending on the type of the dialog, the variables
  // |dialog_|, |pending_dialog_|, and |dialog_callback_| can be present in
  // different combinations.
  //
  // No dialog:
  //   |dialog_|, |pending_dialog_|, and |dialog_callback_| are null.
  // alert() dialog:
  //   Either |dialog_| or |pending_dialog_| is not null. If the dialog is shown
  //   while the tab was foremost, the dialog is be created and a weak pointer
  //   to it is held in |dialog_|. If the dialog is attempted while the tab
  //   is not foremost, the call to create the dialog-to-be is held in
  //   |pending_dialog_| until the tab is brought foremost. At that time the
  //   callback will be made, |pending_dialog_| will be null, and the dialog
  //   will live, referenced by |dialog_|. As for |dialog_callback_|, if the
  //   dialog is shown while the tab was foremost, |dialog_callback_| is not
  //   null. If the dialog was shown while the tab was not foremost, or if the
  //   tab was switched to be non-foremost, the renderer is not held blocked,
  //   and |dialog_callback_| will be null (because it will have been called to
  //   free up the renderer.)
  // confirm() and prompt() dialogs:
  //   Both |dialog_| and |dialog_callback_| are not null. |pending_dialog_| is
  //   null, as only alert() dialogs can be in a pending state.

  // The dialog being displayed on the observed WebContents, if any. At any
  // given time at most one of |dialog_| and |pending_dialog_| can be non-null.
  base::WeakPtr<TabModalDialogView> dialog_;
  base::OnceCallback<base::WeakPtr<TabModalDialogView>()> pending_dialog_;

  // The callback to return a result for a dialog. Not null if the renderer is
  // waiting for a result; null if there is no |dialog_| or if the dialog is an
  // alert() dialog and the callback has already been called.
  content::JavaScriptDialogManager::DialogClosedCallback dialog_callback_;

  // The type of dialog being displayed. Only valid when |dialog_| or
  // |pending_dialog_| is non-null.
  content::JavaScriptDialogType dialog_type_ =
      content::JavaScriptDialogType::JAVASCRIPT_DIALOG_TYPE_ALERT;

  // A closure to be fired when a dialog is shown. For testing only.
  base::OnceClosure dialog_shown_;

  // A closure to be fired when a dialog is dismissed. For testing only.
  DialogDismissedCallback dialog_dismissed_;

  std::unique_ptr<TabModalDialogManagerDelegate> delegate_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_TAB_MODAL_DIALOG_MANAGER_H_
