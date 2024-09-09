// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/javascript_dialogs/ios/tab_modal_dialog_view_ios.h"

#import "content/public/browser/browser_thread.h"
#import "content/public/browser/web_contents.h"

namespace javascript_dialogs {

// static
base::WeakPtr<TabModalDialogViewIOS> TabModalDialogViewIOS::Create(
    content::WebContents* parent_web_contents,
    content::WebContents* alerting_web_contents,
    const std::u16string& title,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback
        callback_on_button_clicked,
    base::OnceClosure callback_on_cancelled) {
  return (new TabModalDialogViewIOS(parent_web_contents, alerting_web_contents,
                                    title, dialog_type, message_text,
                                    default_prompt_text,
                                    std::move(callback_on_button_clicked),
                                    std::move(callback_on_cancelled)))
      ->weak_factory_.GetWeakPtr();
}

// TabModalDialogViewIOS:
TabModalDialogViewIOS::~TabModalDialogViewIOS() {}

void TabModalDialogViewIOS::CloseDialogWithoutCallback() {
  delete this;
}

std::u16string TabModalDialogViewIOS::GetUserInput() {
  return std::u16string();
}

TabModalDialogViewIOS::TabModalDialogViewIOS(
    content::WebContents* parent_web_contents,
    content::WebContents* alerting_web_contents,
    const std::u16string& title,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback
        callback_on_button_clicked,
    base::OnceClosure callback_on_cancelled)
    : callback_on_button_clicked_(std::move(callback_on_button_clicked)),
      callback_on_cancelled_(std::move(callback_on_cancelled)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

}  // namespace javascript_dialogs
