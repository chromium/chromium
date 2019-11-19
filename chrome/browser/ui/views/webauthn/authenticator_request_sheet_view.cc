// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"

#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// Fixed height of the illustration shown in the top half of the sheet.
constexpr int kIllustrationHeight = 148;

// Height of the progress bar style activity indicator shown at the top of some
// sheets.
constexpr int kActivityIndicatorHeight = 4;

using ImageColorScheme = AuthenticatorRequestSheetModel::ImageColorScheme;

}  // namespace

using views::BoxLayout;

AuthenticatorRequestSheetView::AuthenticatorRequestSheetView(
    std::unique_ptr<AuthenticatorRequestSheetModel> model)
    : model_(std::move(model)) {}

AuthenticatorRequestSheetView::~AuthenticatorRequestSheetView() = default;

void AuthenticatorRequestSheetView::ReInitChildViews() {
  RemoveAllChildViews(true /* delete_children */);

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
  return step_specific_content_;
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::BuildStepSpecificContent() {
  return nullptr;
}

void AuthenticatorRequestSheetView::ButtonPressed(views::Button* sender,
                                                  const ui::Event& event) {
  DCHECK_EQ(sender, back_arrow_button_);
  model()->OnBack();
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::CreateIllustrationWithOverlays() {
  const int illustration_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  const gfx::Size illustration_size(illustration_width, kIllustrationHeight);

  // The container view has no layout, so its preferred size is hardcoded to
  // match the size of the image, and all overlays are absolutely positioned.
  auto image_with_overlays = std::make_unique<views::View>();
  image_with_overlays->SetPreferredSize(illustration_size);

  auto image_view = std::make_unique<NonAccessibleImageView>();
  step_illustration_ = image_view.get();
  UpdateIconImageFromModel();
  image_view->SetSize(illustration_size);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  image_with_overlays->AddChildView(image_view.release());

  if (model()->IsActivityIndicatorVisible()) {
    auto activity_indicator = std::make_unique<views::ProgressBar>(
        kActivityIndicatorHeight, false /* allow_round_corner */);
    activity_indicator->SetValue(-1 /* inifinite animation */);
    activity_indicator->SetBackgroundColor(SK_ColorTRANSPARENT);
    activity_indicator->SetPreferredSize(
        gfx::Size(illustration_width, kActivityIndicatorHeight));
    activity_indicator->SizeToPreferredSize();
    image_with_overlays->AddChildView(activity_indicator.release());
  }

  if (model()->IsBackButtonVisible()) {
    auto back_arrow = views::CreateVectorImageButton(this);
    back_arrow->SetFocusForPlatform();
    back_arrow->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_BACK_BUTTON_AUTHENTICATOR_REQUEST_DIALOG));

    // Position the back button so that there is the standard amount of padding
    // between the top/left side of the back button and the dialog borders.
    const gfx::Insets dialog_insets =
        views::LayoutProvider::Get()->GetDialogInsetsForContentType(
            views::CONTROL, views::CONTROL);
    auto color_reference = std::make_unique<views::Label>(
        base::string16(), views::style::CONTEXT_DIALOG_TITLE,
        views::style::STYLE_PRIMARY);
    views::SetImageFromVectorIcon(back_arrow.get(),
                                  vector_icons::kBackArrowIcon,
                                  color_utils::DeriveDefaultIconColor(
                                      color_reference->GetEnabledColor()));
    back_arrow->SizeToPreferredSize();
    back_arrow->SetX(dialog_insets.left());
    back_arrow->SetY(dialog_insets.top());
    back_arrow_button_ =
        image_with_overlays->AddChildView(std::move(back_arrow));
  }

  return image_with_overlays;
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
          views::CONTROL, views::CONTROL)));

  auto label_container = std::make_unique<views::View>();
  label_container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  auto title_label = std::make_unique<views::Label>(
      model()->GetStepTitle(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_PRIMARY);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetAllowCharacterBreak(true);
  label_container->AddChildView(title_label.release());

  base::string16 description = model()->GetStepDescription();
  if (!description.empty()) {
    auto description_label = std::make_unique<views::Label>(
        std::move(description), views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT);
    description_label->SetMultiLine(true);
    description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    description_label->SetAllowCharacterBreak(true);
    label_container->AddChildView(description_label.release());
  }

  base::Optional<base::string16> additional_desciption =
      model()->GetAdditionalDescription();
  if (additional_desciption) {
    auto label = std::make_unique<views::Label>(
        std::move(*additional_desciption),
        views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT);
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetAllowCharacterBreak(true);
    label_container->AddChildView(label.release());
  }

  contents->AddChildView(label_container.release());

  std::unique_ptr<views::View> step_specific_content =
      BuildStepSpecificContent();
  if (step_specific_content) {
    step_specific_content_ = step_specific_content.get();
    contents->AddChildView(step_specific_content.release());
    contents_layout->SetFlexForView(step_specific_content_, 1);
  }

  return contents;
}

void AuthenticatorRequestSheetView::OnThemeChanged() {
  UpdateIconImageFromModel();
}

void AuthenticatorRequestSheetView::UpdateIconImageFromModel() {
  if (!step_illustration_)
    return;

  gfx::IconDescription icon_description(
      model()->GetStepIllustration(GetNativeTheme()->ShouldUseDarkColors()
                                       ? ImageColorScheme::kDark
                                       : ImageColorScheme::kLight),
      0 /* automatic dip_size */, SK_ColorBLACK, base::TimeDelta(),
      gfx::kNoneIcon);
  step_illustration_->SetImage(gfx::CreateVectorIcon(icon_description));
}
