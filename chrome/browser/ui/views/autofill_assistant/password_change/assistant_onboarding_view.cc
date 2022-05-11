// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/assistant_onboarding_view.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

// Factory function to create onboarding prompts on desktop platforms.
AssistantOnboardingPrompt* AssistantOnboardingPrompt::Create(
    AssistantOnboardingController* controller,
    AssistantDisplayDelegate* display_delegate) {
  return new AssistantOnboardingView(controller, display_delegate);
}

AssistantOnboardingView::AssistantOnboardingView(
    AssistantOnboardingController* controller,
    AssistantDisplayDelegate* display_delegate)
    : controller_(controller), display_delegate_(display_delegate) {
  // Since display_delegate_ owns |this|, it must never be a nullptr.
  DCHECK(display_delegate_);
  display_delegate_->SetView(base::WrapUnique(this));
}

AssistantOnboardingView::~AssistantOnboardingView() {
  if (controller_) {
    std::exchange(controller_, nullptr)->OnClose();
  }
}

void AssistantOnboardingView::Show() {
  // TODO(crbug.com/1322387): Set up proper layout and content.
}

void AssistantOnboardingView::OnControllerGone() {
  controller_ = nullptr;
  Close();
}

void AssistantOnboardingView::Close() {
  DCHECK(!controller_);
  display_delegate_->RemoveView();
}

void AssistantOnboardingView::OnAccept() {
  if (controller_) {
    std::exchange(controller_, nullptr)->OnAccept();
  }
  Close();
}

void AssistantOnboardingView::OnCancel() {
  if (controller_) {
    std::exchange(controller_, nullptr)->OnCancel();
  }
  Close();
}

BEGIN_METADATA(AssistantOnboardingView, views::View)
END_METADATA
