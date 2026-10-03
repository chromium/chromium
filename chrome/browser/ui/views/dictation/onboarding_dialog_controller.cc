// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/onboarding_dialog_controller.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/theme_tracking_image_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_delegate.h"

namespace dictation {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingDialogElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingOkButtonElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingCancelButtonElementId);

namespace {
inline constexpr char kOnboardingDialogName[] = "DictationOnboardingDialog";
// Padding between the top of the dialog and the banner image, per the mocks.
inline constexpr int kBannerTopPadding = 20;
// Extra padding between the banner image and the title, per the mocks.
inline constexpr int kBannerBottomPadding = 8;

inline constexpr char kGoogleSans[] = "Google Sans";
inline constexpr char kGoogleSansText[] = "Google Sans Text";

// Returns the font for `text_style`, with its size and weight, in
// `font_family` (Google Sans by default) to match the mocks. Falls back to the
// system font if it isn't installed.
gfx::FontList GetGoogleSansFont(int text_style,
                                const char* font_family = kGoogleSans) {
  const gfx::FontList base = views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_LABEL, text_style);
  return gfx::FontList({font_family}, base.GetFontStyle(), base.GetFontSize(),
                       base.GetFontWeight());
}

// TODO(crbug.com/530962875): Update typography font styles once PM & UX
// reach alignment.
std::unique_ptr<views::View> CreateOnboardingCardView() {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetBetweenChildSpacing(2)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
      .AddChildren(
          // Row 1 (Sparkles) - Top tile with 12px top corners & 0px bottom
          // corners
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetBetweenChildSpacing(12)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetBackground(views::CreateRoundedRectBackground(
                  ui::kColorSysSurface2,
                  gfx::RoundedCornersF(12.0f, 12.0f, 0.0f, 0.0f)))
              .SetInsideBorderInsets(gfx::Insets::VH(12, 16))
              .AddChildren(
                  views::Builder<views::ImageView>()
                      .SetImage(ui::ImageModel::FromVectorIcon(
                          kScreensaverAutoIcon, ui::kColorSysPrimary, 16))
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(1, 0, 0, 0)),
                  views::Builder<views::Label>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_DICTATION_ONBOARDING_BULLET_DATA_SHARING))
                      .SetTextStyle(views::style::STYLE_BODY_4)
                      .SetFontList(GetGoogleSansFont(views::style::STYLE_BODY_4,
                                                     kGoogleSansText))
                      .SetMultiLine(true)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetEnabledColor(ui::kColorSysOnSurface)
                      .SetSubpixelRenderingEnabled(false)),
          // Row 2 (Microphone) - Bottom tile with 0px top corners & 12px bottom
          // corners
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetBetweenChildSpacing(12)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetBackground(views::CreateRoundedRectBackground(
                  ui::kColorSysSurface2,
                  gfx::RoundedCornersF(0.0f, 0.0f, 12.0f, 12.0f)))
              .SetInsideBorderInsets(gfx::Insets::VH(12, 16))
              .AddChildren(
                  views::Builder<views::ImageView>()
                      .SetImage(ui::ImageModel::FromVectorIcon(
                          vector_icons::kMicIcon, ui::kColorSysPrimary, 16))
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(1, 0, 0, 0)),
                  views::Builder<views::Label>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_DICTATION_ONBOARDING_BULLET_MICROPHONE))
                      .SetTextStyle(views::style::STYLE_BODY_4)
                      .SetFontList(GetGoogleSansFont(views::style::STYLE_BODY_4,
                                                     kGoogleSansText))
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
                       .SetBetweenChildSpacing(
                           views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_RELATED_CONTROL_VERTICAL))
                       .Build();

  container->AddChildView(CreateOnboardingCardView());

  std::vector<std::u16string> replacements;
  std::u16string link_text = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  replacements.push_back(link_text);

  std::vector<size_t> offsets;
  std::u16string disclaimer_text = l10n_util::GetStringFUTF16(
      IDS_DICTATION_ONBOARDING_BULLET_DISCLAIMER, replacements, &offsets);
  size_t link_offset = offsets.empty() ? 0 : offsets[0];

  const gfx::FontList disclaimer_font =
      GetGoogleSansFont(views::style::STYLE_BODY_5, kGoogleSansText);

  auto disclaimer_label = std::make_unique<views::StyledLabel>();
  disclaimer_label->SetText(disclaimer_text);
  disclaimer_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  disclaimer_label->SetDefaultTextStyle(views::style::STYLE_BODY_5);
  disclaimer_label->SetDefaultEnabledColorId(ui::kColorSysOnSurfaceVariant);

  auto link_view = std::make_unique<views::Link>(
      link_text, views::style::CONTEXT_LABEL, views::style::STYLE_BODY_5);
  link_view->SetFontList(disclaimer_font);
  link_view->SetCallback(std::move(learn_more_callback));

  views::StyledLabel::RangeStyleInfo link_style;
  link_style.custom_view = link_view.get();
  disclaimer_label->AddCustomView(std::move(link_view));

  // StyledLabel has no default font setter, so style the text around the link.
  views::StyledLabel::RangeStyleInfo text_style;
  text_style.custom_font = disclaimer_font;
  auto add_text_range = [&](size_t start, size_t end) {
    if (start < end) {
      disclaimer_label->AddStyleRange(gfx::Range(start, end), text_style);
    }
  };
  const size_t link_end = link_offset + link_text.length();
  add_text_range(0, link_offset);
  disclaimer_label->AddStyleRange(gfx::Range(link_offset, link_end),
                                  link_style);
  add_text_range(link_end, disclaimer_text.length());

  container->AddChildView(std::move(disclaimer_label));

  return container;
}
}  // namespace

OnboardingDialogController::OnboardingDialogController(tabs::TabInterface& tab)
    : tab_(tab) {
  tab_activation_subscription_ = tab_->RegisterDidActivate(base::BindRepeating(
      &OnboardingDialogController::OnTabActivated, base::Unretained(this)));
}

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
  model_host->set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH));
  const gfx::Insets dialog_insets =
      views::LayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG);
  const int related_control_gap =
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL);
  // The title is left-aligned, so its trailing margin isn't visible. Trim it
  // so the first line fits as in the mocks; including the trailing space that
  // line wrapping counts, it is ~476px, wider than the 472px default.
  constexpr int kTitleTrailingMarginReduction = 8;
  model_host->set_frame_margins(
      {.title = gfx::Insets::TLBR(
           related_control_gap, dialog_insets.left(), 0,
           dialog_insets.right() - kTitleTrailingMarginReduction)});
  model_host->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  widget_ = tab_->GetTabFeatures()->tab_dialog_manager()->CreateAndShowDialog(
      model_host, std::make_unique<tabs::TabDialogManager::Params>());

  if (auto* frame = model_host->GetBubbleFrameView()) {
    if (auto* title_label = views::AsViewClass<views::Label>(frame->title())) {
      title_label->SetTextStyle(views::style::STYLE_HEADLINE_4);
      title_label->SetFontList(
          GetGoogleSansFont(views::style::STYLE_HEADLINE_4));
    }

    // The frame places the header flush with the top of the dialog, so add the
    // banner here (instead of via DialogModel::SetBannerImage()) to give it
    // top padding.
    auto banner_view = std::make_unique<views::ThemeTrackingImageView>(
        ui::ImageModel::FromResourceId(IDR_DICTATION_ONBOARDING_BANNER),
        ui::ImageModel::FromResourceId(IDR_DICTATION_ONBOARDING_BANNER_DARK),
        base::BindRepeating(&views::BubbleDialogDelegate::background_color,
                            base::Unretained(model_host)));
    banner_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kBannerTopPadding, 0, kBannerBottomPadding, 0)));
    // The banner is purely decorative.
    banner_view->GetViewAccessibility().SetIsIgnored(true);
    frame->SetHeaderView(std::move(banner_view));
    model_host->SizeToContents();
  }

  widget_->MakeCloseSynchronous(base::BindOnce(
      &OnboardingDialogController::Close, base::Unretained(this)));

  views::View* initially_focused_view = model_host->GetInitiallyFocusedView();
  CHECK(initially_focused_view);
  initially_focused_view->RequestFocus();
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
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              CreateBodyView(base::BindRepeating(
                  &OnboardingDialogController::OnLearnMoreLinkClicked,
                  weak_ptr_factory_.GetWeakPtr())),
              views::BubbleDialogModelHost::FieldType::kText))
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
                           .SetLabel(l10n_util::GetStringUTF16(
                               IDS_DICTATION_ONBOARDING_BUTTON_CANCEL))
                           .SetId(kDictationOnboardingCancelButtonElementId)
                           .SetStyle(ui::ButtonStyle::kTonal))
      .SetInitiallyFocusedField(kDictationOnboardingOkButtonElementId)
      .SetIsAlertDialog()
      .Build();
}

void OnboardingDialogController::OnLearnMoreLinkClicked() {
  tab_->GetBrowserWindowInterface()->OpenGURL(
      GURL("https://support.google.com/chrome?p=voice_typing"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void OnboardingDialogController::OnDialogAccepted(
    base::OnceClosure complete_callback) {
  std::move(complete_callback).Run();
  Close(views::Widget::ClosedReason::kAcceptButtonClicked);
  // WARNING: `this` is deleted
}

void OnboardingDialogController::Close(views::Widget::ClosedReason reason) {
  DictationFirstRunExitStatus status;
  switch (reason) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      status = DictationFirstRunExitStatus::kCompleted;
      break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      status = DictationFirstRunExitStatus::kCancelled;
      break;
    default:
      status = DictationFirstRunExitStatus::kAbandoned;
      break;
  }
  RecordDictationFirstRunExitStatus(status);

  widget_.reset();
  if (close_callback_) {
    std::move(close_callback_).Run();
  }
  // WARNING: close_callback_ above deletes `this`.
}

void OnboardingDialogController::OnTabActivated(tabs::TabInterface* tab) {
  if (IsShowing()) {
    widget_->StackAtTop();
    widget_->Activate();
    if (widget_->widget_delegate()) {
      auto* view = widget_->widget_delegate()->GetInitiallyFocusedView();
      if (view) {
        view->RequestFocus();
      }
    }
  }
}

}  // namespace dictation
