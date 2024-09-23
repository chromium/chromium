// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "cc/paint/skottie_wrapper.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/lottie/animation.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

// Margins around any illustration.
constexpr int kImageMarginTop = 22;
constexpr int kImageMarginBottom = 2;

template <typename T>
void ConfigureHeaderIllustration(T* illustration, gfx::Size header_size) {
  illustration->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kImageMarginTop, 0, kImageMarginBottom, 0)));
  illustration->SetSize(header_size);
  illustration->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
}

}  // namespace

using views::BoxLayout;

AuthenticatorRequestSheetView::AuthenticatorRequestSheetView(
    std::unique_ptr<AuthenticatorRequestSheetModel> model)
    : model_(std::move(model)) {}

AuthenticatorRequestSheetView::~AuthenticatorRequestSheetView() = default;

void AuthenticatorRequestSheetView::ReInitChildViews() {
  child_views_ = ChildViews();
  RemoveAllChildViews();

  if (model()->IsActivityIndicatorVisible()) {
    constexpr int kActivityIndicatorHeight = 4;
    auto activity_indicator = std::make_unique<views::ProgressBar>();
    activity_indicator->SetPreferredHeight(kActivityIndicatorHeight);
    activity_indicator->SetPreferredCornerRadii(std::nullopt);
    activity_indicator->SetValue(-1 /* infinite animation */);
    activity_indicator->SetBackgroundColor(SK_ColorTRANSPARENT);
    activity_indicator->SetPreferredSize(
        gfx::Size(ChromeLayoutProvider::Get()->GetDistanceMetric(
                      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH),
                  kActivityIndicatorHeight));
    activity_indicator->SizeToPreferredSize();
    // The indicator is positioned absolutely at the top of the dialog.
    activity_indicator->SetProperty(views::kViewIgnoredByLayoutKey, true);
    AddChildView(activity_indicator.release());
  }

  // No need to add further spacing between the upper and lower half. The image
  // is designed to fill the dialog's top half without any border/margins, and
  // the |lower_half| will already contain the standard dialog borders.
  SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(),
      0 /* between_child_spacing */));

  std::unique_ptr<views::View> upper_half = CreateIllustrationWithOverlays();
  std::unique_ptr<views::View> lower_half = CreateContentsBelowIllustration();
  AddChildView(upper_half.release());
  AddChildView(lower_half.release());
  InvalidateLayout();
}

views::View* AuthenticatorRequestSheetView::GetInitiallyFocusedView() {
  if (should_focus_step_specific_content_ == AutoFocus::kYes) {
    return child_views_.step_specific_content_;
  }
  if (accessibility_state_utils::IsScreenReaderEnabled()) {
    // Focus the title label if a screen reader is detected to nudge it to
    // announce the title when the sheet changes.
    return child_views_.title_label_;
  }
  return nullptr;
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::BuildStepSpecificHeader() {
  return nullptr;
}

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorRequestSheetView::BuildStepSpecificContent() {
  return std::make_pair(nullptr, AutoFocus::kNo);
}

int AuthenticatorRequestSheetView::GetSpacingBetweenTitleAndDescription() {
  return views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::CreateIllustrationWithOverlays() {
  constexpr int kImageHeight = 112;
  constexpr int kHeaderHeight =
      kImageHeight + kImageMarginTop + kImageMarginBottom;
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  const gfx::Size header_size(dialog_width, kHeaderHeight);

  // The actual illustration image is set in `UpdateIconImageFromModel`, below,
  // because it's not until that point that we know whether the light or dark
  // illustration should be used.
  View* illustration;
  if (model()->lottie_illustrations()) {
    auto animation = std::make_unique<views::AnimatedImageView>();
    // `AnimatedImageView` will horizontally center if the width is larger than
    // the size from the Lottie file, but the height is just used to truncate
    // the image, so that is disabled with a very large value.
    animation->SetPreferredSize(gfx::Size(dialog_width, 9999));
    ConfigureHeaderIllustration(animation.get(), header_size);
    child_views_.step_illustration_animation_ = animation.get();
    illustration = animation.release();
  } else if (model()->vector_illustrations()) {
    auto image_view = std::make_unique<NonAccessibleImageView>();
    ConfigureHeaderIllustration(image_view.get(), header_size);
    child_views_.step_illustration_image_ = image_view.get();
    illustration = image_view.release();
  } else {
    return std::make_unique<views::View>();
  }

  // The container view has no layout, so its preferred size is hardcoded to
  // match the size of the header, and all overlays are absolutely positioned.
  auto header_view = std::make_unique<views::View>();
  header_view->SetPreferredSize(header_size);
  header_view->AddChildView(illustration);

  if (GetWidget()) {
    UpdateIconImageFromModel();
  }

  return header_view;
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::CreateContentsBelowIllustration() {
  auto contents = std::make_unique<views::View>();
  contents->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  contents->SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl,
          views::DialogContentType::kControl)));

  std::unique_ptr<views::View> step_specific_header = BuildStepSpecificHeader();
  if (step_specific_header) {
    child_views_.step_specific_header_ =
        contents->AddChildView(step_specific_header.release());

    if (model()->lottie_illustrations() || model()->vector_illustrations()) {
      auto insets = contents->GetBorder()->GetInsets();
      insets.set_top(0);
      contents->SetBorder(views::CreateEmptyBorder(insets));
    }
  }

  // GPM PIN dialogs have a different spacing, 4px.
  auto label_container = std::make_unique<views::View>();
  label_container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(),
      GetSpacingBetweenTitleAndDescription()));

  std::unique_ptr<views::View> step_specific_content;
  // Compute `should_focus_step_specific_content_` before setting `title_label`
  // so that the focus behavior of the `title_label` is set correctly.
  std::tie(step_specific_content, should_focus_step_specific_content_) =
      BuildStepSpecificContent();
  DCHECK(should_focus_step_specific_content_ == AutoFocus::kNo ||
         step_specific_content);

  const std::u16string title = model()->GetStepTitle();
  if (!title.empty()) {
    auto title_label = std::make_unique<views::Label>(
        title, views::style::CONTEXT_DIALOG_TITLE,
        views::style::STYLE_HEADLINE_4);
    title_label->SetMultiLine(true);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
    title_label->SetAllowCharacterBreak(true);
    if (accessibility_state_utils::IsScreenReaderEnabled() &&
        should_focus_step_specific_content_ == AutoFocus::kNo) {
      title_label->SetFocusBehavior(FocusBehavior::ALWAYS);
    }
    child_views_.title_label_ =
        label_container->AddChildView(title_label.release());
  }

  std::u16string description = model()->GetStepDescription();
  if (!description.empty()) {
    auto description_label = std::make_unique<views::Label>(
        std::move(description), views::style::CONTEXT_DIALOG_BODY_TEXT);
    description_label->SetMultiLine(true);
    description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    description_label->SetAllowCharacterBreak(true);
    label_container->AddChildView(description_label.release());
  }

  for (const std::u16string& msg : model()->GetAdditionalDescriptions()) {
    if (msg.empty()) {
      continue;
    }
    auto label = std::make_unique<views::Label>(
        msg, views::style::CONTEXT_DIALOG_BODY_TEXT);
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetAllowCharacterBreak(true);
    label_container->AddChildView(label.release());
  }

  contents->AddChildView(label_container.release());

  auto content_error_and_hint_view = std::make_unique<views::View>();
  BoxLayout* content_error_and_hint_layout =
      content_error_and_hint_view->SetLayoutManager(std::make_unique<BoxLayout>(
          BoxLayout::Orientation::kVertical, gfx::Insets(),
          views::LayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  if (step_specific_content) {
    child_views_.step_specific_content_ =
        content_error_and_hint_view->AddChildView(
            step_specific_content.release());
    content_error_and_hint_layout->SetFlexForView(
        child_views_.step_specific_content_, 1);
  }

  std::u16string error = model()->GetError();
  if (!error.empty()) {
    auto error_label = std::make_unique<views::Label>(
        std::move(error), views::style::CONTEXT_LABEL, STYLE_RED);
    error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    error_label->SetMultiLine(true);
    child_views_.error_label_ =
        content_error_and_hint_view->AddChildView(std::move(error_label));
  }

  std::u16string hint = model()->GetHint();
  if (!hint.empty()) {
    auto hint_label = std::make_unique<views::Label>(
        std::move(hint), views::style::CONTEXT_LABEL, views::style::STYLE_HINT);
    hint_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    child_views_.hint_label_ =
        content_error_and_hint_view->AddChildView(std::move(hint_label));
  }

  contents->AddChildView(content_error_and_hint_view.release());
  return contents;
}

void AuthenticatorRequestSheetView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateIconImageFromModel();
}

void AuthenticatorRequestSheetView::UpdateIconImageFromModel() {
  const bool is_dark = color_utils::IsDark(
      GetColorProvider()->GetColor(ui::kColorDialogBackground));
  if (child_views_.step_illustration_image_) {
    child_views_.step_illustration_image_->SetImage(
        ui::ImageModel::FromVectorIcon(
            model()->vector_illustrations()->get(is_dark),
            gfx::kPlaceholderColor));
  } else if (child_views_.step_illustration_animation_) {
    const int lottie_id = model()->lottie_illustrations()->get(is_dark);
    std::optional<std::vector<uint8_t>> lottie_bytes =
        ui::ResourceBundle::GetSharedInstance().GetLottieData(lottie_id);
    scoped_refptr<cc::SkottieWrapper> skottie =
        cc::SkottieWrapper::UnsafeCreateSerializable(std::move(*lottie_bytes));
    child_views_.step_illustration_animation_->SetAnimatedImage(
        std::make_unique<lottie::Animation>(skottie));
    child_views_.step_illustration_animation_->SizeToPreferredSize();
    child_views_.step_illustration_animation_->Play();
  }
}

BEGIN_METADATA(AuthenticatorRequestSheetView)
END_METADATA
