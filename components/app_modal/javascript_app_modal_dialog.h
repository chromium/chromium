// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_MODAL_JAVASCRIPT_APP_MODAL_DIALOG_H_
#define COMPONENTS_APP_MODAL_JAVASCRIPT_APP_MODAL_DIALOG_H_

#include <map>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace app_modal {

class NativeAppModalDialog;

// Extra data for JavaScript dialogs to add Chrome-only features.
class ChromeJavaScriptDialogExtraData {
 public:
  ChromeJavaScriptDialogExtraData();

  // True if the user has already seen a JavaScript dialog from the WebContents.
  bool has_already_shown_a_dialog_;

  // True if the user has decided to block future JavaScript dialogs.
  bool suppress_javascript_messages_;
};

// A controller + model class for JavaScript alert, confirm, prompt, and
// onbeforeunload dialog boxes.
class JavaScriptAppModalDialog {
 public:
  typedef std::map<void*, ChromeJavaScriptDialogExtraData> ExtraDataMap;

  JavaScriptAppModalDialog(
      content::WebContents* web_contents,
      ExtraDataMap* extra_data_map,
      const base::string16& title,
      content::JavaScriptDialogType javascript_dialog_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      bool display_suppress_checkbox,
      bool is_before_unload_dialog,
      bool is_reload,
      content::JavaScriptDialogManager::DialogClosedCallback callback);
  ~JavaScriptAppModalDialog();

  // Called by the AppModalDialogQueue to show this dialog.
  void ShowModalDialog();

  // Called by the AppModalDialogQueue to activate the dialog.
  void ActivateModalDialog();

  // Closes the dialog if it is showing.
  void CloseModalDialog();

  // Returns true if the dialog is still valid. As dialogs are created they are
  // added to the AppModalDialogQueue. When the current modal dialog finishes
  // and it's time to show the next dialog in the queue IsValid is invoked.
  // If IsValid returns false the dialog is deleted and not shown.
  bool IsValid();

  // Invalidates the dialog, therefore causing it to not be shown when its turn
  // to be shown comes around.
  void Invalidate();

  // Callbacks from NativeDialog when the user accepts or cancels the dialog.
  void OnCancel(bool suppress_js_messages);
  void OnAccept(const base::string16& prompt_text, bool suppress_js_messages);

  // NOTE: This is only called under Views, and should be removed. Any critical
  // work should be done in OnCancel or OnAccept. See crbug.com/63732 for more.
  void OnClose();

  // Used only for testing. The dialog will use the given text when notifying
  // its delegate instead of whatever the UI reports.
  void SetOverridePromptText(const base::string16& prompt_text);

  // Accessors.
  base::string16 title() const { return title_; }
  NativeAppModalDialog* native_dialog() const { return native_dialog_; }
  content::WebContents* web_contents() const { return web_contents_; }
  content::JavaScriptDialogType javascript_dialog_type() const {
    return javascript_dialog_type_;
  }
  base::string16 message_text() const { return message_text_; }
  base::string16 default_prompt_text() const { return default_prompt_text_; }
  bool display_suppress_checkbox() const { return display_suppress_checkbox_; }
  bool is_before_unload_dialog() const { return is_before_unload_dialog_; }
  bool is_reload() const { return is_reload_; }

 private:
  // Notifies the delegate with the result of the dialog.
  void NotifyDelegate(bool success, const base::string16& prompt_text,
                      bool suppress_js_messages);

  void CallDialogClosedCallback(bool success,
                                const base::string16& prompt_text);

  // Completes dialog handling, shows next modal dialog from the queue.
  // TODO(beng): Get rid of this method.
  void CompleteDialog();

  // The title of the dialog.
  base::string16 title_;

  // // True if CompleteDialog was called.
  bool completed_;

  // False if the dialog should no longer be shown, e.g. because the underlying
  // tab navigated away while the dialog was queued.
  bool valid_;

  // // The toolkit-specific implementation of the app modal dialog box.
  NativeAppModalDialog* native_dialog_;

  // The WebContents that opened this dialog.
  content::WebContents* web_contents_;

  // A map of extra Chrome-only data associated with the delegate_. Can be
  // inspected via |extra_data_map_[web_contents_]|.
  ExtraDataMap* extra_data_map_;

  // Information about the message box is held in the following variables.
  const content::JavaScriptDialogType javascript_dialog_type_;
  base::string16 message_text_;
  base::string16 default_prompt_text_;
  bool display_suppress_checkbox_;
  bool is_before_unload_dialog_;
  bool is_reload_;

  content::JavaScriptDialogManager::DialogClosedCallback callback_;

  // Used only for testing. Specifies alternative prompt text that should be
  // used when notifying the delegate, if |use_override_prompt_text_| is true.
  base::string16 override_prompt_text_;
  bool use_override_prompt_text_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptAppModalDialog);
};

// An interface to observe that a modal dialog is shown.
class AppModalDialogObserver {
 public:
  AppModalDialogObserver();
  virtual ~AppModalDialogObserver();

  // Called when the modal dialog is shown.
  virtual void Notify(JavaScriptAppModalDialog* dialog) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppModalDialogObserver);
};

}  // namespace app_modal

#endif  // COMPONENTS_APP_MODAL_JAVASCRIPT_APP_MODAL_DIALOG_H_
