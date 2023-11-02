// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_android.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/javascript_dialogs/android/tab_modal_dialog_view_android.h"

JavaScriptTabModalDialogManagerDelegateAndroid::
    JavaScriptTabModalDialogManagerDelegateAndroid(
        content::WebContents* web_contents)
    : web_contents_(web_contents) {}

JavaScriptTabModalDialogManagerDelegateAndroid::
    ~JavaScriptTabModalDialogManagerDelegateAndroid() = default;

// Note on the two callbacks: |dialog_callback_on_button_clicked| is for the
// case where user responds to the dialog. |dialog_callback_on_cancelled| is
// for the case where user cancels the dialog without interacting with the
// dialog (e.g. clicks the navigate back button on Android).
base::WeakPtr<javascript_dialogs::TabModalDialogView>
JavaScriptTabModalDialogManagerDelegateAndroid::CreateNewDialog(
    content::WebContents* alerting_web_contents,
    const std::u16string& title,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback
        callback_on_button_clicked,
    base::OnceClosure callback_on_cancelled) {
  return javascript_dialogs::TabModalDialogViewAndroid::Create(
      web_contents_, alerting_web_contents, title, dialog_type, message_text,
      default_prompt_text, std::move(callback_on_button_clicked),
      std::move(callback_on_cancelled));
}

void JavaScriptTabModalDialogManagerDelegateAndroid::WillRunDialog() {}

void JavaScriptTabModalDialogManagerDelegateAndroid::DidCloseDialog() {}

void JavaScriptTabModalDialogManagerDelegateAndroid::SetTabNeedsAttention(
    bool attention) {}

bool JavaScriptTabModalDialogManagerDelegateAndroid::IsWebContentsForemost() {
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents_);
  if (tab_model) {
    return tab_model->IsActiveModel() &&
           tab_model->GetActiveWebContents() == web_contents_;
  } else {
    // If tab model is not found (e.g. single tab model), fall back to check
    // whether the tab for this web content is interactive.
    TabAndroid* tab = TabAndroid::FromWebContents(web_contents_);
    return tab && tab->IsUserInteractable();
  }
}

bool JavaScriptTabModalDialogManagerDelegateAndroid::IsApp() {
  return false;
}
