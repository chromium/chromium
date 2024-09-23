// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"

#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

MediaGalleryCheckboxView::MediaGalleryCheckboxView(
    const MediaGalleryPrefInfo& pref_info,
    int trailing_vertical_space,
    views::ContextMenuController* menu_controller) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(0, dialog_insets.left(),
                                                       trailing_vertical_space,
                                                       dialog_insets.right())));
  if (menu_controller)
    set_context_menu_controller(menu_controller);

  checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
      pref_info.GetGalleryDisplayName(), views::Button::PressedCallback()));
  if (menu_controller)
    checkbox_->set_context_menu_controller(menu_controller);
  checkbox_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  std::u16string tooltip_text = pref_info.GetGalleryTooltip();
  checkbox_->SetTooltipText(tooltip_text);

  std::u16string details = pref_info.GetGalleryAdditionalDetails();
  secondary_text_ = AddChildView(std::make_unique<views::Label>(
      details, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  if (menu_controller)
    secondary_text_->set_context_menu_controller(menu_controller);
  secondary_text_->SetVisible(details.length() > 0);
  secondary_text_->SetElideBehavior(gfx::ELIDE_HEAD);
  secondary_text_->SetTooltipText(tooltip_text);
  secondary_text_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL),
      0, 0)));
}

MediaGalleryCheckboxView::~MediaGalleryCheckboxView() = default;

void MediaGalleryCheckboxView::Layout(PassKey) {
  LayoutSuperclass<views::BoxLayoutView>(this);
  if (GetPreferredSize().width() <= GetLocalBounds().width())
    return;

  // If box layout doesn't fit, do custom layout. The secondary text should take
  // up at most half of the space and the checkbox can take up whatever is left.
  int checkbox_width = checkbox_->GetPreferredSize().width();
  int secondary_text_width =
      secondary_text_
          ->GetPreferredSize(views::SizeBounds(secondary_text_->width(), {}))
          .width();
  if (!secondary_text_->GetVisible())
    secondary_text_width = 0;

  gfx::Rect area = GetContentsBounds();

  if (secondary_text_width > area.width() / 2) {
    secondary_text_width =
        std::max(area.width() / 2, area.width() - checkbox_width);
  }
  checkbox_width = area.width() - secondary_text_width;

  checkbox_->SetBounds(area.x(), area.y(), checkbox_width, area.height());
  if (secondary_text_->GetVisible()) {
    secondary_text_->SetBounds(checkbox_->x() + checkbox_width, area.y(),
                               secondary_text_width, area.height());
  }
}

BEGIN_METADATA(MediaGalleryCheckboxView)
END_METADATA
