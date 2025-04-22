// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/feature_first_run/feature_first_run_helper.h"

#include "base/notreached.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace feature_first_run {

namespace {

// Builds a DialogModel for a generic FFR dialog.
std::unique_ptr<ui::DialogModel> CreateGenericFeatureFirstRunDialogModel(
    std::u16string title) {
  return ui::DialogModel::Builder()
      .SetTitle(title)
      .AddOkButton(base::DoNothing(),
                   ui::DialogModel::Button::Params().SetLabel(
                       l10n_util::GetStringUTF16(IDS_APP_TURN_ON)))
      .AddCancelButton(base::DoNothing())
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

}  // namespace

std::unique_ptr<RichControlsContainerView> CreateInfoBoxContainer(
    const std::u16string& title,
    const std::u16string& description,
    const gfx::VectorIcon& vector_icon,
    InfoBoxPosition position) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int container_padding = layout_provider->GetDistanceMetric(
      DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_PADDING);
  const int icon_size = layout_provider->GetDistanceMetric(
      DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_ICON_SIZE);

  auto container_view = std::make_unique<RichControlsContainerView>();
  container_view->SetTitle(title);
  container_view->AddSecondaryLabel(description);
  container_view->SetIcon(ui::ImageModel::FromVectorIcon(
      vector_icon, kColorFeatureFirstRunIconColor, icon_size));
  container_view->SetBackground(views::CreateRoundedRectBackground(
      kColorFeatureFirstRunInfoContainerBackground,
      CalculateInfoBoxBorderRadius(position)));
  container_view->SetInteriorMargin(
      gfx::Insets::VH(container_padding, container_padding));

  return container_view;
}

views::Widget* ShowFeatureFirstRunDialog(std::u16string title,
                                         content::WebContents* web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab || !tab->CanShowModalUI()) {
    return nullptr;
  }

  return constrained_window::ShowWebModal(
      CreateGenericFeatureFirstRunDialogModel(title), web_contents);
}

}  // namespace feature_first_run
