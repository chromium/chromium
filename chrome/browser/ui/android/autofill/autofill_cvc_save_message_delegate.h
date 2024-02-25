// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_CVC_SAVE_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_CVC_SAVE_MESSAGE_DELEGATE_H_

#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Enables showing the CVC save prompt in a Message UI if CVC was entered during
// checkout for an existing server card. Forwards the calls from the Message UI.
class AutofillCvcSaveMessageDelegate {
 public:
  explicit AutofillCvcSaveMessageDelegate(content::WebContents* web_contents);

  AutofillCvcSaveMessageDelegate(const AutofillCvcSaveMessageDelegate&) =
      delete;
  AutofillCvcSaveMessageDelegate& operator=(
      const AutofillCvcSaveMessageDelegate&) = delete;

  virtual ~AutofillCvcSaveMessageDelegate();

  // Shows the message.
  void ShowMessage(const AutofillSaveCardUiInfo& ui_info,
                   std::unique_ptr<AutofillSaveCardDelegateAndroid> delegate);
  // Callbacks for user decision.
  void OnMessageAccepted();
  void OnMessageCancelled();
  void OnMessageDismissed(messages::DismissReason dismiss_reason);

 private:
  // Deletes the save card delegate on a new task. This is done to avoid that
  // the save card delegate can delete itself.
  void DeleteSaveCardDelegateSoon();

  // Dismisses the message if it is visible.
  void DismissMessage();

  const raw_ptr<content::WebContents> web_contents_;
  // Provides callbacks for the message.
  std::unique_ptr<AutofillSaveCardDelegateAndroid> save_card_delegate_;
  // Delegate of a toast style popup showing on the top of the screen. It has
  // value whenever a message is to be shown. It is reset when the message is
  // dismissed.
  std::optional<messages::MessageWrapper> message_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_CVC_SAVE_MESSAGE_DELEGATE_H_
