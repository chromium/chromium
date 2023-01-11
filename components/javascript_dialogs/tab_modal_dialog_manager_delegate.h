// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_TAB_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_TAB_MODAL_DIALOG_MANAGER_DELEGATE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace javascript_dialogs {

class TabModalDialogView;

// This interface provides platform-specific controller functionality to
// TabModalDialogManager.
class TabModalDialogManagerDelegate {
 public:
  virtual ~TabModalDialogManagerDelegate() = default;

  // Factory function for creating a tab modal view.
  virtual base::WeakPtr<TabModalDialogView> CreateNewDialog(
      content::WebContents* alerting_web_contents,
      const std::u16string& title,
      content::JavaScriptDialogType dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_closed_callback) = 0;

  // Called when a dialog is about to be shown.
  virtual void WillRunDialog() = 0;

  // Called when a dialog has been hidden.
  virtual void DidCloseDialog() = 0;

  // Called when a tab should indicate to the user that it needs attention, such
  // as when an alert fires from a background tab.
  virtual void SetTabNeedsAttention(bool attention) = 0;

  // Should return true if the web contents is foremost (i.e. the active tab in
  // the active browser window).
  virtual bool IsWebContentsForemost() = 0;

  // Should return true if this web contents is an app window, such as a PWA.
  virtual bool IsApp() = 0;
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_TAB_MODAL_DIALOG_MANAGER_DELEGATE_H_
