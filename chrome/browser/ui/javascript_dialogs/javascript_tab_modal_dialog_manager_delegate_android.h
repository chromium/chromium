// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager_delegate.h"

namespace content {
class WebContents;
}

class JavaScriptTabModalDialogManagerDelegateAndroid
    : public javascript_dialogs::TabModalDialogManagerDelegate {
 public:
  explicit JavaScriptTabModalDialogManagerDelegateAndroid(
      content::WebContents* web_contents);
  ~JavaScriptTabModalDialogManagerDelegateAndroid() override;

  JavaScriptTabModalDialogManagerDelegateAndroid(
      const JavaScriptTabModalDialogManagerDelegateAndroid& other) = delete;
  JavaScriptTabModalDialogManagerDelegateAndroid& operator=(
      const JavaScriptTabModalDialogManagerDelegateAndroid& other) = delete;

  // javascript_dialogs::TabModalDialogManagerDelegate
  base::WeakPtr<javascript_dialogs::TabModalDialogView> CreateNewDialog(
      content::WebContents* alerting_web_contents,
      const std::u16string& title,
      content::JavaScriptDialogType dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_closed_callback) override;
  void WillRunDialog() override;
  void DidCloseDialog() override;
  void SetTabNeedsAttention(bool attention) override;
  bool IsWebContentsForemost() override;
  bool IsApp() override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_ANDROID_H_
