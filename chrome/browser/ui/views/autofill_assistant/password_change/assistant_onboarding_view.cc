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
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/layout/layout_provider.h"

// Factory function to create onboarding prompts on desktop platforms.
base::WeakPtr<AssistantOnboardingPrompt> AssistantOnboardingPrompt::Create(
    base::WeakPtr<AssistantOnboardingController> controller,
    content::WebContents* web_contents) {
  return (new AssistantOnboardingView(controller, web_contents))->GetWeakPtr();
}

AssistantOnboardingView::AssistantOnboardingView(
    base::WeakPtr<AssistantOnboardingController> controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  DCHECK(controller_);
}

AssistantOnboardingView::~AssistantOnboardingView() = default;

void AssistantOnboardingView::Show() {
  DCHECK(controller_);

  InitDelegate();
  InitDialog();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void AssistantOnboardingView::OnControllerGone() {
  controller_ = nullptr;
  if (GetWidget()) {
    // Trigger own destruction.
    GetWidget()->Close();
  } else {
    // If this is not owned by a widget, delete itself.
    delete this;
  }
}

void AssistantOnboardingView::InitDelegate() {
  SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 controller_->GetOnboardingInformation().button_accept_text);
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 controller_->GetOnboardingInformation().button_cancel_text);

  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetAcceptCallback(
      base::BindOnce(&AssistantOnboardingController::OnAccept, controller_));
  SetCancelCallback(
      base::BindOnce(&AssistantOnboardingController::OnCancel, controller_));
  SetCloseCallback(
      base::BindOnce(&AssistantOnboardingController::OnClose, controller_));
}

void AssistantOnboardingView::InitDialog() {
  // TODO(crbug.com/1322387): Populate dialog with views for image, texts, and
  // learn more link.
}

base::WeakPtr<AssistantOnboardingView> AssistantOnboardingView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(AssistantOnboardingView, views::DialogDelegateView)
END_METADATA
