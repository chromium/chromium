// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_CVC_SAVE_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_CVC_SAVE_MESSAGE_DELEGATE_H_

#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Enables showing the CVC save prompt in a Message UI if CVC was entered during
// checkout for an existing server card.
class AutofillCvcSaveMessageDelegate {
 public:
  explicit AutofillCvcSaveMessageDelegate(content::WebContents* web_contents);

  AutofillCvcSaveMessageDelegate(const AutofillCvcSaveMessageDelegate&) =
      delete;
  AutofillCvcSaveMessageDelegate& operator=(
      const AutofillCvcSaveMessageDelegate&) = delete;

  virtual ~AutofillCvcSaveMessageDelegate();

  // Shows the message.
  virtual void ShowMessage();

 private:
  // Dismisses the message if it is visible.
  void DismissMessage();

  const raw_ptr<content::WebContents> web_contents_;

  // Delegate of a toast style popup showing on the top of the screen. It has
  // value whenever a message is to be shown. It is reset when the message is
  // dismissed.
  std::optional<messages::MessageWrapper> message_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_CVC_SAVE_MESSAGE_DELEGATE_H_
