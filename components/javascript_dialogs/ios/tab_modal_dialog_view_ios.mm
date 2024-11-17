// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/javascript_dialogs/ios/tab_modal_dialog_view_ios.h"

#import "base/strings/sys_string_conversions.h"
#import "content/public/browser/browser_thread.h"
#import "content/public/browser/web_contents.h"
#import "content/public/common/javascript_dialog_type.h"
#import "ui/gfx/native_widget_types.h"

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
TabModalDialogViewIOS::~TabModalDialogViewIOS() = default;

void TabModalDialogViewIOS::CloseDialogWithoutCallback() {
  coordinator_ = nullptr;
  delete this;
}

std::u16string TabModalDialogViewIOS::GetUserInput() {
  return [coordinator_ promptText];
}

void TabModalDialogViewIOS::Accept(const std::u16string& prompt_text) {
  if (callback_on_button_clicked_) {
    std::move(callback_on_button_clicked_).Run(true, prompt_text);
  }
  delete this;
}

void TabModalDialogViewIOS::Cancel() {
  if (callback_on_cancelled_) {
    std::move(callback_on_cancelled_).Run();
  }
  delete this;
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

  gfx::NativeWindow native_window =
      parent_web_contents->GetTopLevelNativeWindow();

  coordinator_ = [[JavascriptDialogViewCoordinator alloc]
      initWithBaseViewController:native_window.Get().rootViewController
                      dialogView:this
                      dialogType:dialog_type
                           title:base::SysUTF16ToNSString(title)
                     messageText:base::SysUTF16ToNSString(message_text)
               defaultPromptText:base::SysUTF16ToNSString(default_prompt_text)];
}

}  // namespace javascript_dialogs
