// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_IOS_TAB_MODAL_DIALOG_VIEW_IOS_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_IOS_TAB_MODAL_DIALOG_VIEW_IOS_H_

#import <memory>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "components/javascript_dialogs/ios/javascript_dialog_view_coordinator.h"
#import "components/javascript_dialogs/tab_modal_dialog_view.h"
#import "content/public/browser/javascript_dialog_manager.h"

@class JavascriptDialogViewCoordinator;

namespace javascript_dialogs {

// An iOS version of the JavaScript dialog referring to the Android
// implementation.
class TabModalDialogViewIOS : public TabModalDialogView {
 public:
  TabModalDialogViewIOS(const TabModalDialogViewIOS&) = delete;
  TabModalDialogViewIOS& operator=(const TabModalDialogViewIOS&) = delete;

  ~TabModalDialogViewIOS() override;

  static base::WeakPtr<TabModalDialogViewIOS> Create(
      content::WebContents* parent_web_contents,
      content::WebContents* alerting_web_contents,
      const std::u16string& title,
      content::JavaScriptDialogType dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback
          callback_on_button_clicked,
      base::OnceClosure callback_on_cancelled);

  // TabModalDialogView:
  void CloseDialogWithoutCallback() override;
  std::u16string GetUserInput() override;

  void Accept(const std::u16string& prompt_text);
  void Cancel();

 private:
  TabModalDialogViewIOS(content::WebContents* parent_web_contents,
                        content::WebContents* alerting_web_contents,
                        const std::u16string& title,
                        content::JavaScriptDialogType dialog_type,
                        const std::u16string& message_text,
                        const std::u16string& default_prompt_text,
                        content::JavaScriptDialogManager::DialogClosedCallback
                            callback_on_button_clicked,
                        base::OnceClosure callback_on_cancelled);

  std::unique_ptr<TabModalDialogViewIOS> dialog_;
  JavascriptDialogViewCoordinator* __strong coordinator_;

  content::JavaScriptDialogManager::DialogClosedCallback
      callback_on_button_clicked_;
  base::OnceClosure callback_on_cancelled_;

  base::WeakPtrFactory<TabModalDialogViewIOS> weak_factory_{this};
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_IOS_TAB_MODAL_DIALOG_VIEW_IOS_H_
