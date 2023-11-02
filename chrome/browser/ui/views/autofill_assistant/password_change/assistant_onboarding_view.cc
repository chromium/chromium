// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/assistant_onboarding_view.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/autofill_assistant/password_change/apc_utils.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace content {
class WebContents;
}  // namespace content

namespace {

// Ratios of element width and dialog width.
constexpr double kAssistantLogoScaleFactor = 0.2;
constexpr double kTitleScaleFactor = 0.8;
constexpr double kDescriptionScaleFactor = 0.8;
constexpr double kSeparatorScaleFactor = 0.8;
constexpr double kConsentTestScaleFactor = 1.0;

}  // namespace

// Factory function to create onboarding prompts on desktop platforms.
base::WeakPtr<AssistantOnboardingPrompt> AssistantOnboardingPrompt::Create(
    base::WeakPtr<AssistantOnboardingController> controller) {
  return (new AssistantOnboardingView(controller))->GetWeakPtr();
}

AssistantOnboardingView::AssistantOnboardingView(
    base::WeakPtr<AssistantOnboardingController> controller)
    : controller_(controller) {
  DCHECK(controller_);
}

AssistantOnboardingView::~AssistantOnboardingView() = default;

void AssistantOnboardingView::Show(content::WebContents* web_contents) {
  DCHECK(controller_);

  InitDelegate();
  InitDialog();
  constrained_window::ShowWebModalDialogViews(this, web_contents);
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
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          controller_->GetOnboardingInformation().button_accept_text_id));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(
          controller_->GetOnboardingInformation().button_cancel_text_id));

  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  set_margins(views::LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  SetAcceptCallback(base::BindOnce(&AssistantOnboardingView::OnAccept,
                                   weak_ptr_factory_.GetWeakPtr()));
  SetCancelCallback(
      base::BindOnce(&AssistantOnboardingController::OnCancel, controller_));
  SetCloseCallback(
      base::BindOnce(&AssistantOnboardingController::OnClose, controller_));
}

void AssistantOnboardingView::InitDialog() {
  // IMPORTANT: If any additional text elements are added, the resource ids
  // of the strings must be included in the data filled in `OnAccept()`.

  // The dialog is not expected to be resized, so for our purposes, a
  // `BoxLayout` is sufficient.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(0);

  const int dialog_width = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);

  auto ConvertScaleFactorToMargin = [dialog_width](double scale_factor) -> int {
    return static_cast<int>((1.0 - scale_factor) * dialog_width / 2.0);
  };

  // The icon. The placeholder color is only used on non-branded developer
  // builds.
  AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(gfx::CreateVectorIcon(
              GetAssistantIconOrFallback(),
              kAssistantLogoScaleFactor * dialog_width, gfx::kPlaceholderColor))
          .SetID(static_cast<int>(DialogViewID::HEADER_ICON))
          .Build());

  // The title.
  AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              controller_->GetOnboardingInformation().title_id))
          .SetTextContext(views::style::TextContext::CONTEXT_DIALOG_TITLE)
          .SetTextStyle(views::style::TextStyle::STYLE_PRIMARY)
          .SetMultiLine(true)
          .SetProperty(
              views::kMarginsKey,
              gfx::Insets::TLBR(
                  /*top=*/views::LayoutProvider::Get()->GetDistanceMetric(
                      views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                  /*left=*/ConvertScaleFactorToMargin(kTitleScaleFactor),
                  /*bottom=*/0,
                  /*right=*/ConvertScaleFactorToMargin(kTitleScaleFactor)))
          .SetID(static_cast<int>(DialogViewID::TITLE))
          .Build());

  // The description.
  AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              controller_->GetOnboardingInformation().description_id))
          .SetTextContext(views::style::TextContext::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::TextStyle::STYLE_SECONDARY)
          .SetMultiLine(true)
          .SetProperty(
              views::kMarginsKey,
              gfx::Insets::TLBR(
                  views::LayoutProvider::Get()->GetDistanceMetric(
                      views::DISTANCE_RELATED_CONTROL_VERTICAL),
                  ConvertScaleFactorToMargin(kDescriptionScaleFactor), 0,
                  ConvertScaleFactorToMargin(kDescriptionScaleFactor)))
          .SetID(static_cast<int>(DialogViewID::DESCRIPTION))
          .Build());

  AddChildView(
      views::Builder<views::Separator>()
          .SetPreferredLength(dialog_width)
          .SetOrientation(views::Separator::Orientation::kHorizontal)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(
                           views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                           ConvertScaleFactorToMargin(kSeparatorScaleFactor),
                           views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                           ConvertScaleFactorToMargin(kSeparatorScaleFactor)))
          .Build());

  // Get the offset of the "Learn more" text to create a link style.
  size_t offset = 0;
  const std::u16string learn_more_title = l10n_util::GetStringUTF16(
      controller_->GetOnboardingInformation().learn_more_title_id);
  const std::u16string consent_text = l10n_util::GetStringFUTF16(
      controller_->GetOnboardingInformation().consent_text_id, learn_more_title,
      &offset);
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &AssistantOnboardingController::OnLearnMoreClicked, controller_));

  // The actual consent text with the "Learn more" link.
  AddChildView(
      views::Builder<views::StyledLabel>()
          .SetText(consent_text)
          .SetTextContext(views::style::TextContext::CONTEXT_DIALOG_BODY_TEXT)
          .SetDefaultTextStyle(views::style::TextStyle::STYLE_HINT)
          .SetProperty(
              views::kMarginsKey,
              gfx::Insets::TLBR(
                  0, ConvertScaleFactorToMargin(kConsentTestScaleFactor), 0,
                  ConvertScaleFactorToMargin(kConsentTestScaleFactor)))
          .SetID(static_cast<int>(DialogViewID::CONSENT_TEXT))
          .AddStyleRange(gfx::Range(offset, offset + learn_more_title.length()),
                         link_style)
          .Build());
}

void AssistantOnboardingView::OnAccept() {
  // IMPORTANT: If any additional text elements are added, then these must be
  // included here to ensure that the audit record is complete.
  if (controller_) {
    const AssistantOnboardingInformation& model =
        controller_->GetOnboardingInformation();
    controller_->OnAccept(model.button_accept_text_id,
                          {model.title_id, model.description_id,
                           model.consent_text_id, model.learn_more_title_id});
  }
}

base::WeakPtr<AssistantOnboardingView> AssistantOnboardingView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(AssistantOnboardingView, views::DialogDelegateView)
END_METADATA
