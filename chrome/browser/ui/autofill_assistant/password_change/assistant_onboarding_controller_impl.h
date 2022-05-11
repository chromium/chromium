// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_CONTROLLER_IMPL_H_

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"

#include "base/callback.h"
#include "base/memory/raw_ptr.h"

class OnboardingPrompt;

// Implementation of the |AssistantOnboardingController| interface that keeps
// a raw pointer to an |AssistantOnboardingPrompt| (i.e. the view component),
// but does not own anything. As a result, it needs to be informed of the
// destruction of the prompt by calling one of its |On*()| methods.
class AssistantOnboardingControllerImpl : public AssistantOnboardingController {
 public:
  explicit AssistantOnboardingControllerImpl(
      const AssistantOnboardingInformation& onboarding_information);
  ~AssistantOnboardingControllerImpl() override;

  // OnboardingController:
  void Show(AssistantOnboardingPrompt* prompt, Callback callback) override;
  // For the below "On*" methods, the controller does not take care of closing
  // the view - this is done by the view itself.
  void OnAccept() override;
  void OnCancel() override;
  void OnClose() override;
  const AssistantOnboardingInformation& GetOnboardingInformation() override;

 private:
  // If the controller has a non-null |OnboardingPrompt|, notify its
  // |OnControllerGone()| method and null the controller's reference to the
  // prompt.
  void ClosePrompt();

  // The data representing the "model" behind the controller.
  const AssistantOnboardingInformation onboarding_information_;

  // Callback triggered when dialog is accepted, canceled or closed.
  Callback callback_;

  // A reference to the view implementing the |OnboardingPrompt| interface.
  // Since the view might outlive the controller, it must call one of the
  // |OnAccept()|, |OnCancel()|, or |OnClose()| methods before destruction
  // so that the controller can invalidate its reference to it.
  raw_ptr<AssistantOnboardingPrompt> prompt_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_CONTROLLER_IMPL_H_
