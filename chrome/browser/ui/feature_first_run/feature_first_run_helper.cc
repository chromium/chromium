// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/feature_first_run/feature_first_run_helper.h"

#include <memory>
#include <string>

#include "base/notreached.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace feature_first_run {

namespace {

// Builds a DialogModel for a generic FFR dialog.
std::unique_ptr<ui::DialogModel> CreateGenericFeatureFirstRunDialogModel(
    std::u16string title,
    ui::ImageModel banner,
    ui::ImageModel dark_mode_banner,
    std::unique_ptr<views::View> content_view,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
  return ui::DialogModel::Builder()
      .SetTitle(std::move(title))
      .SetBannerImage(std::move(banner), std::move(dark_mode_banner))
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              std::move(content_view),
              views::BubbleDialogModelHost::FieldType::kText),
          kFeatureFirstRunDialogContentViewElementId)
      .AddOkButton(std::move(accept_callback),
                   ui::DialogModel::Button::Params().SetLabel(
                       l10n_util::GetStringUTF16(IDS_APP_TURN_ON)))
      .AddCancelButton(std::move(cancel_callback))
      .Build();
}

gfx::RoundedCornersF CalculateInfoBoxBorderRadius(InfoBoxPosition position) {
  const int rounded_corner_radius =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_ROUNDED_BORDER_RADIUS);

  switch (position) {
    case InfoBoxPosition::kStart:
      return gfx::RoundedCornersF(rounded_corner_radius, rounded_corner_radius,
                                  0, 0);
    case InfoBoxPosition::kMiddle:
      return gfx::RoundedCornersF(0);
    case InfoBoxPosition::kEnd:
      return gfx::RoundedCornersF(0, 0, rounded_corner_radius,
                                  rounded_corner_radius);
  }
  NOTREACHED();
}

std::unique_ptr<RichControlsContainerView>
CreateInfoBoxContainerWithoutDescription(std::u16string title,
                                         const gfx::VectorIcon& vector_icon,
                                         InfoBoxPosition position) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int container_padding = layout_provider->GetDistanceMetric(
      DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_PADDING);
  const int icon_size = layout_provider->GetDistanceMetric(
      DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_ICON_SIZE);

  auto container_view = std::make_unique<RichControlsContainerView>();
  container_view->SetTitle(std::move(title));
  container_view->SetIconImageSizeAndMargins({icon_size, icon_size},
                                             gfx::Insets::VH(2, 0));
  container_view->SetIcon(ui::ImageModel::FromVectorIcon(
      vector_icon, kColorFeatureFirstRunIconColor, icon_size));
  container_view->SetBackground(views::CreateRoundedRectBackground(
      kColorFeatureFirstRunInfoContainerBackground,
      CalculateInfoBoxBorderRadius(position)));
  container_view->SetInteriorMargin(
      gfx::Insets::VH(container_padding, container_padding));

  return container_view;
}

}  // namespace

std::unique_ptr<RichControlsContainerView> CreateInfoBoxContainer(
    std::u16string title,
    std::u16string description,
    const gfx::VectorIcon& vector_icon,
    InfoBoxPosition position) {
  auto container_view = CreateInfoBoxContainerWithoutDescription(
      std::move(title), vector_icon, position);
  container_view->AddSecondaryLabel(std::move(description));

  return container_view;
}

std::unique_ptr<RichControlsContainerView> CreateInfoBoxContainerWithLearnMore(
    std::u16string title,
    int description_id,
    const std::u16string& learn_more,
    base::RepeatingClosure learn_more_callback,
    const gfx::VectorIcon& vector_icon,
    InfoBoxPosition position) {
  auto container_view = CreateInfoBoxContainerWithoutDescription(
      std::move(title), vector_icon, position);

  size_t offset;
  std::u16string full_description =
      l10n_util::GetStringFUTF16(description_id, learn_more, &offset);

  gfx::Range learn_more_link_range(offset, offset + learn_more.size());
  auto learn_more_link = views::StyledLabel::RangeStyleInfo::CreateForLink(
      std::move(learn_more_callback));

  auto* description_view =
      container_view->AddSecondaryStyledLabel(full_description);
  description_view->AddStyleRange(learn_more_link_range,
                                  std::move(learn_more_link));

  return container_view;
}

std::unique_ptr<views::View> CreateDialogContentViewContainer() {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int container_top_padding = layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  const int space_between = layout_provider->GetDistanceMetric(
      DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_VERTICAL);

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetBetweenChildSpacing(space_between)
      .SetInsideBorderInsets(gfx::Insets().set_top(container_top_padding))
      .Build();
}

views::Widget* ShowFeatureFirstRunDialog(
    std::u16string title,
    ui::ImageModel banner,
    ui::ImageModel dark_mode_banner,
    std::unique_ptr<views::View> content_view,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    content::WebContents* web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab || !tab->CanShowModalUI()) {
    return nullptr;
  }

  return constrained_window::ShowWebModal(
      CreateGenericFeatureFirstRunDialogModel(
          std::move(title), std::move(banner), std::move(dark_mode_banner),
          std::move(content_view), std::move(accept_callback),
          std::move(cancel_callback)),
      web_contents);
}

}  // namespace feature_first_run
