// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller_impl.h"

#include <utility>

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"

AssistantOnboardingControllerImpl::AssistantOnboardingControllerImpl(
    const AssistantOnboardingInformation& onboarding_information)
    : onboarding_information_(onboarding_information) {}

AssistantOnboardingControllerImpl::~AssistantOnboardingControllerImpl() {
  ClosePrompt();
}

void AssistantOnboardingControllerImpl::Show(AssistantOnboardingPrompt* prompt,
                                             Callback callback) {
  // If there is another prompt that is controlled by |this|, close it.
  ClosePrompt();

  callback_ = std::move(callback);
  prompt_ = prompt;
  prompt_->Show();
}

void AssistantOnboardingControllerImpl::OnAccept() {
  if (prompt_) {
    prompt_ = nullptr;
    std::move(callback_).Run(true);
  }
}

void AssistantOnboardingControllerImpl::OnCancel() {
  if (prompt_) {
    prompt_ = nullptr;
    std::move(callback_).Run(false);
  }
}

void AssistantOnboardingControllerImpl::OnClose() {
  if (prompt_) {
    prompt_ = nullptr;
    std::move(callback_).Run(false);
  }
}

const AssistantOnboardingInformation&
AssistantOnboardingControllerImpl::GetOnboardingInformation() {
  return onboarding_information_;
}

void AssistantOnboardingControllerImpl::ClosePrompt() {
  if (prompt_) {
    std::exchange(prompt_, nullptr)->OnControllerGone();
    std::move(callback_).Run(false);
  }
}

// Factory function, declared in `assistant_onboarding_controller.h`.
// static
std::unique_ptr<AssistantOnboardingController>
AssistantOnboardingController::Create(
    const AssistantOnboardingInformation& onboarding_information) {
  return std::make_unique<AssistantOnboardingControllerImpl>(
      onboarding_information);
}
