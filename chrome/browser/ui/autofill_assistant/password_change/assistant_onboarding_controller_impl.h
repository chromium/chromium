// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_CONTROLLER_IMPL_H_

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"

#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

class OnboardingPrompt;

// Implementation of the `AssistantOnboardingController` interface that keeps
// a weak pointer to an `AssistantOnboardingPrompt` (i.e. the view component),
class AssistantOnboardingControllerImpl : public AssistantOnboardingController {
 public:
  explicit AssistantOnboardingControllerImpl(
      const AssistantOnboardingInformation& onboarding_information,
      content::WebContents* web_contents);
  ~AssistantOnboardingControllerImpl() override;

  // OnboardingController:
  void Show(base::WeakPtr<AssistantOnboardingPrompt> prompt,
            Callback callback) override;
  // For the below `On*` methods, the controller does not take care of closing
  // the view - this is done by the view itself.
  void OnAccept(int confirmation_grd_id,
                const std::vector<int>& description_grd_ids) override;
  void OnCancel() override;
  void OnClose() override;
  void OnLearnMoreClicked() override;
  const AssistantOnboardingInformation& GetOnboardingInformation() override;
  base::WeakPtr<AssistantOnboardingController> GetWeakPtr() override;

 private:
  // Closes the `OnboardingPrompt` associated with this controller
  // (if one exists).
  void ClosePrompt();

  // The data representing the "model" behind the controller.
  const AssistantOnboardingInformation onboarding_information_;

  // Callback triggered when dialog is accepted, canceled or closed.
  Callback callback_;

  // The `WebContents` for which the dialog is supposed to show.
  raw_ptr<content::WebContents> web_contents_;

  // A weak pointer to the view implementing the `OnboardingPrompt`
  // interface.
  base::WeakPtr<AssistantOnboardingPrompt> prompt_ = nullptr;

  // A factory for weak pointers to the controller.
  base::WeakPtrFactory<AssistantOnboardingControllerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_CONTROLLER_IMPL_H_
