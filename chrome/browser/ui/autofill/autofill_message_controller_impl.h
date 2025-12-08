// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_CONTROLLER_IMPL_H_

#include <memory>

#include "base/containers/unique_ptr_adapters.h"
#include "chrome/browser/ui/autofill/autofill_message_controller.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// AutofillMessageControllerImpl is used to control the lifecycle of autofill
// Messages on Android. This includes handling the show, dismiss and callbacks
// of the messages.
class AutofillMessageControllerImpl : public AutofillMessageController {
 public:
  explicit AutofillMessageControllerImpl(content::WebContents* web_contents);
  AutofillMessageControllerImpl(const AutofillMessageControllerImpl&) = delete;
  AutofillMessageControllerImpl& operator=(
      const AutofillMessageControllerImpl&) = delete;
  ~AutofillMessageControllerImpl() override;

  // AutofillMessageController:
  void Show(std::unique_ptr<AutofillMessageModel> message_model) override;

 private:
  friend class AutofillMessageControllerTestApi;

  // Callback for when the action button on the message is clicked.
  void OnActionClicked(AutofillMessageModel* message_model_ptr);

  // Callback for when the message is dismissed.
  void OnDismissed(AutofillMessageModel* message_model_ptr,
                   messages::DismissReason reason);

  void Dismiss();

  const raw_ref<content::WebContents> web_contents_;
  std::set<std::unique_ptr<AutofillMessageModel>, base::UniquePtrComparator>
      message_models_;
  base::WeakPtrFactory<AutofillMessageControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_CONTROLLER_IMPL_H_
