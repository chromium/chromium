// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "cc/paint/skottie_wrapper.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "device/fido/features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/lottie/animation.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Margin between the top of the dialog and the start of any illustration.
constexpr int kImageMarginTop = 22;

template <typename T>
void ConfigureHeaderIllustration(T* illustration, gfx::Size header_size) {
  illustration->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kImageMarginTop, 0, kImageMarginTop, 0)));
  illustration->SetSize(header_size);
  illustration->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
}

bool ScreenReaderModeEnabled() {
  return base::FeatureList::IsEnabled(device::kWebAuthnScreenReaderMode) &&
         accessibility_state_utils::IsScreenReaderEnabled();
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
  if (ScreenReaderModeEnabled()) {
    // Focus the title label if a screen reader is detected to nudge it to
    // announce the title when the sheet changes.
    return child_views_.title_label_;
  }
  return nullptr;
}

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorRequestSheetView::BuildStepSpecificContent() {
  return std::make_pair(nullptr, AutoFocus::kNo);
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::CreateIllustrationWithOverlays() {
  constexpr int kImageHeight = 112, kImageMarginBottom = 2;
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
    animation->SetPreferredSize(gfx::Size(dialog_width, kImageHeight));
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

  if (model()->IsActivityIndicatorVisible()) {
    constexpr int kActivityIndicatorHeight = 4;
    auto activity_indicator = std::make_unique<views::ProgressBar>();
    activity_indicator->SetPreferredHeight(kActivityIndicatorHeight);
    activity_indicator->SetPreferredCornerRadii(absl::nullopt);
    activity_indicator->SetValue(-1 /* infinite animation */);
    activity_indicator->SetBackgroundColor(SK_ColorTRANSPARENT);
    activity_indicator->SetPreferredSize(
        gfx::Size(dialog_width, kActivityIndicatorHeight));
    activity_indicator->SizeToPreferredSize();
    header_view->AddChildView(activity_indicator.release());
  }

  if (GetWidget()) {
    UpdateIconImageFromModel();
  }

  return header_view;
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::CreateContentsBelowIllustration() {
  auto contents = std::make_unique<views::View>();
  BoxLayout* contents_layout =
      contents->SetLayoutManager(std::make_unique<BoxLayout>(
          BoxLayout::Orientation::kVertical, gfx::Insets(),
          views::LayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  contents->SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl,
          views::DialogContentType::kControl)));

  auto label_container = std::make_unique<views::View>();
  label_container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  const std::u16string title = model()->GetStepTitle();
  if (!title.empty()) {
    auto title_label = std::make_unique<views::Label>(
        title, views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
    title_label->SetMultiLine(true);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetAccessibleRole(ax::mojom::Role::kHeading);
    title_label->SetAllowCharacterBreak(true);
    if (features::IsChromeRefresh2023()) {
      title_label->SetTextStyle(views::style::STYLE_HEADLINE_4);
    }
    if (ScreenReaderModeEnabled() &&
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

  std::u16string additional_desciption = model()->GetAdditionalDescription();
  if (!additional_desciption.empty()) {
    auto label =
        std::make_unique<views::Label>(std::move(additional_desciption),
                                       views::style::CONTEXT_DIALOG_BODY_TEXT);
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetAllowCharacterBreak(true);
    label_container->AddChildView(label.release());
  }

  contents->AddChildView(label_container.release());

  std::unique_ptr<views::View> step_specific_content;
  std::tie(step_specific_content, should_focus_step_specific_content_) =
      BuildStepSpecificContent();
  DCHECK(should_focus_step_specific_content_ == AutoFocus::kNo ||
         step_specific_content);
  if (step_specific_content) {
    child_views_.step_specific_content_ = step_specific_content.get();
    contents->AddChildView(step_specific_content.release());
    contents_layout->SetFlexForView(child_views_.step_specific_content_, 1);
  }

  std::u16string error = model()->GetError();
  if (!error.empty()) {
    auto error_label = std::make_unique<views::Label>(
        std::move(error), views::style::CONTEXT_LABEL, STYLE_RED);
    error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    error_label->SetMultiLine(true);
    child_views_.error_label_ = contents->AddChildView(std::move(error_label));
  }

  return contents;
}

void AuthenticatorRequestSheetView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateIconImageFromModel();
}

void AuthenticatorRequestSheetView::UpdateIconImageFromModel() {
  const bool is_dark = GetNativeTheme()->ShouldUseDarkColors();
  if (child_views_.step_illustration_image_) {
    child_views_.step_illustration_image_->SetImage(
        ui::ImageModel::FromVectorIcon(
            model()->vector_illustrations()->get(is_dark),
            gfx::kPlaceholderColor));
  } else if (child_views_.step_illustration_animation_) {
    const int lottie_id = model()->lottie_illustrations()->get(is_dark);
    absl::optional<std::vector<uint8_t>> lottie_bytes =
        ui::ResourceBundle::GetSharedInstance().GetLottieData(lottie_id);
    scoped_refptr<cc::SkottieWrapper> skottie =
        cc::SkottieWrapper::CreateSerializable(std::move(*lottie_bytes));
    child_views_.step_illustration_animation_->SetAnimatedImage(
        std::make_unique<lottie::Animation>(skottie));
    child_views_.step_illustration_animation_->SizeToPreferredSize();
    child_views_.step_illustration_animation_->Play();
  }
}

BEGIN_METADATA(AuthenticatorRequestSheetView, views::View)
END_METADATA
