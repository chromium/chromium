// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller_impl.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

AssistantOnboardingControllerImpl::AssistantOnboardingControllerImpl(
    const AssistantOnboardingInformation& onboarding_information,
    content::WebContents* web_contents)
    : onboarding_information_(onboarding_information),
      web_contents_(web_contents) {}

AssistantOnboardingControllerImpl::~AssistantOnboardingControllerImpl() {
  ClosePrompt();
}

void AssistantOnboardingControllerImpl::Show(
    base::WeakPtr<AssistantOnboardingPrompt> prompt,
    Callback callback) {
  // If there is another prompt that is controlled by `this`, close it.
  ClosePrompt();

  callback_ = std::move(callback);
  prompt_ = prompt;
  prompt_->Show(web_contents_);
}

void AssistantOnboardingControllerImpl::OnAccept(
    int confirmation_grd_id,
    const std::vector<int>& description_grd_ids) {
  if (prompt_) {
    prompt_ = nullptr;
    std::move(callback_).Run(true, confirmation_grd_id, description_grd_ids);
  }
}

void AssistantOnboardingControllerImpl::OnCancel() {
  if (prompt_) {
    prompt_ = nullptr;
    std::move(callback_).Run(false, /*confirmation_grd_id=*/absl::nullopt,
                             /*description_grd_ids=*/{});
  }
}

void AssistantOnboardingControllerImpl::OnClose() {
  if (prompt_) {
    prompt_ = nullptr;
    std::move(callback_).Run(false, /*confirmation_grd_id=*/absl::nullopt,
                             /*description_grd_ids=*/{});
  }
}

void AssistantOnboardingControllerImpl::OnLearnMoreClicked() {
  NavigateParams params(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      GetOnboardingInformation().learn_more_url,
      ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

const AssistantOnboardingInformation&
AssistantOnboardingControllerImpl::GetOnboardingInformation() {
  return onboarding_information_;
}

base::WeakPtr<AssistantOnboardingController>
AssistantOnboardingControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AssistantOnboardingControllerImpl::ClosePrompt() {
  if (prompt_) {
    std::exchange(prompt_, nullptr)->OnControllerGone();
    std::move(callback_).Run(false, /*confirmation_grd_id=*/absl::nullopt,
                             /*description_grd_ids=*/{});
  }
}

// Factory function, declared in `assistant_onboarding_controller.h`.
// static
std::unique_ptr<AssistantOnboardingController>
AssistantOnboardingController::Create(
    const AssistantOnboardingInformation& onboarding_information,
    content::WebContents* web_contents) {
  return std::make_unique<AssistantOnboardingControllerImpl>(
      onboarding_information, web_contents);
}
