// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_MESSAGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_MESSAGE_CONTROLLER_H_

#include <memory>

#include "base/containers/unique_ptr_adapters.h"
#include "chrome/browser/ui/autofill/payments/autofill_message_model.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// AutofillMessageController is used to control the lifecycle of autofill
// Messages on Android. This includes handling the show, dismiss and callbacks
// of the messages.
class AutofillMessageController {
 public:
  explicit AutofillMessageController(content::WebContents* web_contents);
  AutofillMessageController(const AutofillMessageController&) = delete;
  AutofillMessageController& operator=(const AutofillMessageController&) =
      delete;
  virtual ~AutofillMessageController();

  // Show a new message. If an existing message is already showing, dismiss that
  // message and show the new one.
  virtual void Show(std::unique_ptr<AutofillMessageModel> message_model);
  // Callback for when the action button on the message is clicked.
  void OnActionClicked(AutofillMessageModel* message_model_ptr);
  // Callback for when the message is dismissed.
  void OnDismissed(AutofillMessageModel* message_model_ptr,
                   messages::DismissReason reason);

 private:
  friend class AutofillMessageControllerTestApi;

  void Dismiss();

  const raw_ref<content::WebContents> web_contents_;
  std::set<std::unique_ptr<AutofillMessageModel>, base::UniquePtrComparator>
      message_models_;
  base::WeakPtrFactory<AutofillMessageController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_MESSAGE_CONTROLLER_H_
