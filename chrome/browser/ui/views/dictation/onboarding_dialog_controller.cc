// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/onboarding_dialog_controller.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace dictation {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingDialogElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingOkButtonElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingCancelButtonElementId);

namespace {
inline constexpr char kOnboardingDialogName[] = "DictationOnboardingDialog";

// TODO(crbug.com/530962875): Update typography font styles once PM & UX
// reach alignment.
std::unique_ptr<views::View> CreateOnboardingCardView() {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetBetweenChildSpacing(2)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
      .AddChildren(
          // Row 1 (Microphone) - Top tile with 16px top corners & 4px bottom
          // corners
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetBetweenChildSpacing(12)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetBackground(views::CreateRoundedRectBackground(
                  ui::kColorSysNeutralContainer,
                  gfx::RoundedCornersF(16.0f, 16.0f, 0.0f, 0.0f)))
              .SetInsideBorderInsets(gfx::Insets(16))
              .AddChildren(
                  views::Builder<views::ImageView>()
                      .SetImage(ui::ImageModel::FromVectorIcon(
                          vector_icons::kMicIcon, ui::kColorSysPrimary, 20))
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(2, 0, 0, 0)),
                  views::Builder<views::Label>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_DICTATION_ONBOARDING_BULLET_MICROPHONE))
                      .SetTextStyle(views::style::STYLE_BODY_4)
                      .SetLineHeight(18)
                      .SetMultiLine(true)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetEnabledColor(ui::kColorSysOnSurface)
                      .SetSubpixelRenderingEnabled(false)),
          // Row 2 (Sparkles) - Bottom tile with 0px top corners & 16px bottom
          // corners
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetBetweenChildSpacing(12)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetBackground(views::CreateRoundedRectBackground(
                  ui::kColorSysNeutralContainer,
                  gfx::RoundedCornersF(0.0f, 0.0f, 16.0f, 16.0f)))
              .SetInsideBorderInsets(gfx::Insets(16))
              .AddChildren(
                  // TODO(crbug.com/530949784): Add speech-spark icon to
                  // src-internal and replace vector_icons::kLightbulbIcon.
                  views::Builder<views::ImageView>()
                      .SetImage(ui::ImageModel::FromVectorIcon(
                          vector_icons::kLightbulbIcon, ui::kColorSysPrimary,
                          20))
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(2, 0, 0, 0)),
                  views::Builder<views::Label>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_DICTATION_ONBOARDING_BULLET_DATA_SHARING))
                      .SetTextStyle(views::style::STYLE_BODY_4)
                      .SetLineHeight(18)
                      .SetMultiLine(true)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetEnabledColor(ui::kColorSysOnSurface)
                      .SetSubpixelRenderingEnabled(false)))
      .Build();
}

std::unique_ptr<views::View> CreateBodyView(
    base::RepeatingClosure learn_more_callback) {
  auto container = views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetBetweenChildSpacing(20)
                       .Build();

  container->AddChildView(CreateOnboardingCardView());

  std::vector<std::u16string> replacements;
  std::u16string link_text = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  replacements.push_back(link_text);

  std::vector<size_t> offsets;
  std::u16string disclaimer_text = l10n_util::GetStringFUTF16(
      IDS_DICTATION_ONBOARDING_BULLET_DISCLAIMER, replacements, &offsets);
  size_t link_offset = offsets.empty() ? 0 : offsets[0];

  auto disclaimer_label = std::make_unique<views::StyledLabel>();
  disclaimer_label->SetText(disclaimer_text);
  disclaimer_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  disclaimer_label->SetDefaultTextStyle(views::style::STYLE_BODY_5);
  disclaimer_label->SetDefaultEnabledColorId(ui::kColorSysOnSurfaceSubtle);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          std::move(learn_more_callback));
  disclaimer_label->AddStyleRange(
      gfx::Range(link_offset, link_offset + link_text.length()), link_style);

  container->AddChildView(std::move(disclaimer_label));

  return container;
}
}  // namespace

OnboardingDialogController::OnboardingDialogController(tabs::TabInterface& tab)
    : tab_(tab) {}

OnboardingDialogController::~OnboardingDialogController() {
  if (IsShowing()) {
    Close(views::Widget::ClosedReason::kUnspecified);
  }
}

void OnboardingDialogController::Show(base::OnceClosure complete_callback,
                                      base::OnceClosure close_callback) {
  if (IsShowing()) {
    return;
  }
  close_callback_ = std::move(close_callback);

  // The widget will own `model_host` through DialogDelegate.
  views::BubbleDialogModelHost* model_host =
      views::BubbleDialogModelHost::CreateModal(
          CreateDialogModel(std::move(complete_callback)),
          ui::mojom::ModalType::kChild)
          .release();
  model_host->set_fixed_width(512);
  model_host->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  widget_ = tab_->GetTabFeatures()->tab_dialog_manager()->CreateAndShowDialog(
      model_host, std::make_unique<tabs::TabDialogManager::Params>());

  if (auto* frame = model_host->GetBubbleFrameView()) {
    if (auto* title_label = views::AsViewClass<views::Label>(frame->title())) {
      title_label->SetTextStyle(views::style::STYLE_HEADLINE_5);
    }
  }

  widget_->MakeCloseSynchronous(base::BindOnce(
      &OnboardingDialogController::Close, base::Unretained(this)));
}

bool OnboardingDialogController::IsShowing() const {
  return widget_ && !widget_->IsClosed();
}

std::unique_ptr<ui::DialogModel> OnboardingDialogController::CreateDialogModel(
    base::OnceClosure complete_callback) {
  return ui::DialogModel::Builder()
      .SetInternalName(kOnboardingDialogName)
      .SetTitle(l10n_util::GetStringUTF16(IDS_DICTATION_ONBOARDING_TITLE))
      .SetElementIdentifier(kDictationOnboardingDialogElementId)
      .SetBannerImage(
          ui::ImageModel::FromResourceId(IDR_DICTATION_ONBOARDING_BANNER),
          ui::ImageModel::FromResourceId(IDR_DICTATION_ONBOARDING_BANNER_DARK))
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              CreateBodyView(base::BindRepeating(
                  &OnboardingDialogController::OnLearnMoreLinkClicked,
                  weak_ptr_factory_.GetWeakPtr())),
              views::BubbleDialogModelHost::FieldType::kControl))
      .AddOkButton(base::BindOnce(&OnboardingDialogController::OnDialogAccepted,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(complete_callback)),
                   ui::DialogModel::Button::Params()
                       .SetLabel(l10n_util::GetStringUTF16(
                           IDS_DICTATION_ONBOARDING_BUTTON_ACCEPT))
                       .SetId(kDictationOnboardingOkButtonElementId)
                       .SetStyle(ui::ButtonStyle::kProminent))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModel::Button::Params()
                           .SetLabel(l10n_util::GetStringUTF16(IDS_NO_THANKS))
                           .SetId(kDictationOnboardingCancelButtonElementId)
                           .SetStyle(ui::ButtonStyle::kTonal))
      .Build();
}

// TODO(crbug.com/530948293): add Learn More url here
void OnboardingDialogController::OnLearnMoreLinkClicked() {
  tab_->GetBrowserWindowInterface()->OpenGURL(
      GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void OnboardingDialogController::OnDialogAccepted(
    base::OnceClosure complete_callback) {
  std::move(complete_callback).Run();
  Close(views::Widget::ClosedReason::kAcceptButtonClicked);
  // WARNING: `this` is deleted
}

void OnboardingDialogController::Close(views::Widget::ClosedReason reason) {
  widget_.reset();
  if (close_callback_) {
    std::move(close_callback_).Run();
  }
  // WARNING: close_callback_ above deletes `this`.
}

}  // namespace dictation
