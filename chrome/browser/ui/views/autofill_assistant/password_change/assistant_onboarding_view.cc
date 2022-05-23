// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/assistant_onboarding_view.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {
// Ratios of element width and dialog width.
constexpr double kAssistantLogoScaleFactor = 0.2;
}  // namespace

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

  set_margins(views::LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  SetAcceptCallback(
      base::BindOnce(&AssistantOnboardingController::OnAccept, controller_));
  SetCancelCallback(
      base::BindOnce(&AssistantOnboardingController::OnCancel, controller_));
  SetCloseCallback(
      base::BindOnce(&AssistantOnboardingController::OnClose, controller_));
}

void AssistantOnboardingView::InitDialog() {
  // The dialog is not expected to be resized, so for our purposes, a
  // `BoxLayout` is sufficient.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // TODO(crbug.com/1322387): Set spacing between children.

  const int dialog_width = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& assistant_icon = kAssistantIcon;
#else
  // Only developer builds will ever use this branch and the color used in
  // `FromVectorIcon` below.
  const gfx::VectorIcon& assistant_icon = kProductIcon;
#endif
  AddChildView(std::make_unique<views::ImageView>())
      ->SetImage(
          gfx::CreateVectorIcon(assistant_icon, gfx::kPlaceholderColor,
                                kAssistantLogoScaleFactor * dialog_width));

  // TODO(crbug.com/1322387): Populate dialog with views for texts and the
  // learn more link.
}

base::WeakPtr<AssistantOnboardingView> AssistantOnboardingView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(AssistantOnboardingView, views::DialogDelegateView)
END_METADATA
