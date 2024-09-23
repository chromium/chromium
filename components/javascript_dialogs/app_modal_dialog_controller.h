// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_CONTROLLER_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_CONTROLLER_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace javascript_dialogs {

class AppModalDialogView;

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
class AppModalDialogController {
 public:
  typedef std::map<void*, ChromeJavaScriptDialogExtraData> ExtraDataMap;

  AppModalDialogController(
      content::WebContents* web_contents,
      ExtraDataMap* extra_data_map,
      const std::u16string& title,
      content::JavaScriptDialogType javascript_dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      bool display_suppress_checkbox,
      bool is_before_unload_dialog,
      bool is_reload,
      content::JavaScriptDialogManager::DialogClosedCallback callback);

  AppModalDialogController(const AppModalDialogController&) = delete;
  AppModalDialogController& operator=(const AppModalDialogController&) = delete;

  ~AppModalDialogController();

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
  void OnAccept(const std::u16string& prompt_text, bool suppress_js_messages);

  // NOTE: This is only called under Views, and should be removed. Any critical
  // work should be done in OnCancel or OnAccept. See crbug.com/63732 for more.
  void OnClose();

  // Used only for testing. The dialog will use the given text when notifying
  // its delegate instead of whatever the UI reports.
  void SetOverridePromptText(const std::u16string& prompt_text);

  // Accessors.
  std::u16string title() const { return title_; }
  AppModalDialogView* view() const { return view_; }
  content::WebContents* web_contents() const { return web_contents_; }
  content::JavaScriptDialogType javascript_dialog_type() const {
    return javascript_dialog_type_;
  }
  std::u16string message_text() const { return message_text_; }
  std::u16string default_prompt_text() const { return default_prompt_text_; }
  bool display_suppress_checkbox() const { return display_suppress_checkbox_; }
  bool is_before_unload_dialog() const { return is_before_unload_dialog_; }
  bool is_reload() const { return is_reload_; }

 private:
  // Notifies the delegate with the result of the dialog.
  void NotifyDelegate(bool success,
                      const std::u16string& prompt_text,
                      bool suppress_js_messages);

  void CallDialogClosedCallback(bool success,
                                const std::u16string& prompt_text);

  // Completes dialog handling, shows next modal dialog from the queue.
  // TODO(beng): Get rid of this method.
  void CompleteDialog();

  // The title of the dialog.
  const std::u16string title_;

  // False if the dialog should no longer be shown, e.g. because the underlying
  // tab navigated away while the dialog was queued.
  bool valid_;

  // The toolkit-specific implementation of the app modal dialog box. When
  // non-null, |view_| owns |this|.
  raw_ptr<AppModalDialogView> view_;

  // The WebContents that opened this dialog.
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_;

  // A map of extra Chrome-only data associated with the delegate_. Can be
  // inspected via |extra_data_map_[web_contents_]|.
  raw_ptr<ExtraDataMap, LeakedDanglingUntriaged> extra_data_map_;

  // Information about the message box is held in the following variables.
  const content::JavaScriptDialogType javascript_dialog_type_;
  const std::u16string message_text_;
  const std::u16string default_prompt_text_;
  const bool display_suppress_checkbox_;
  const bool is_before_unload_dialog_;
  const bool is_reload_;

  content::JavaScriptDialogManager::DialogClosedCallback callback_;

  // Used only for testing. Specifies alternative prompt text that should be
  // used when notifying the delegate, if |use_override_prompt_text_| is true.
  std::u16string override_prompt_text_;
  bool use_override_prompt_text_;
};

// An interface to observe that a modal dialog is shown.
class AppModalDialogObserver {
 public:
  AppModalDialogObserver();

  AppModalDialogObserver(const AppModalDialogObserver&) = delete;
  AppModalDialogObserver& operator=(const AppModalDialogObserver&) = delete;

  virtual ~AppModalDialogObserver();

  // Called when the modal dialog is shown.
  virtual void Notify(AppModalDialogController* dialog) = 0;
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_CONTROLLER_H_
