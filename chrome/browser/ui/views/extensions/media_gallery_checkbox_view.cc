// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"

#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Equal to the #9F9F9F color used in spec (note WebUI color is #999).
const SkColor kDeemphasizedTextColor = SkColorSetRGB(159, 159, 159);

}  // namespace

MediaGalleryCheckboxView::MediaGalleryCheckboxView(
    const MediaGalleryPrefInfo& pref_info,
    int trailing_vertical_space,
    views::ButtonListener* button_listener,
    views::ContextMenuController* menu_controller) {
  DCHECK(button_listener != NULL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);
  SetBorder(views::CreateEmptyBorder(
      0, dialog_insets.left(), trailing_vertical_space, dialog_insets.right()));
  if (menu_controller)
    set_context_menu_controller(menu_controller);

  checkbox_ =
      new views::Checkbox(pref_info.GetGalleryDisplayName(), button_listener);
  if (menu_controller)
    checkbox_->set_context_menu_controller(menu_controller);
  checkbox_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  base::string16 tooltip_text = pref_info.GetGalleryTooltip();
  checkbox_->SetTooltipText(tooltip_text);

  base::string16 details = pref_info.GetGalleryAdditionalDetails();
  secondary_text_ = new views::Label(details);
  if (menu_controller)
    secondary_text_->set_context_menu_controller(menu_controller);
  secondary_text_->SetVisible(details.length() > 0);
  secondary_text_->SetEnabledColor(kDeemphasizedTextColor);
  secondary_text_->SetElideBehavior(gfx::ELIDE_HEAD);
  secondary_text_->SetTooltipText(tooltip_text);
  secondary_text_->SetBorder(views::CreateEmptyBorder(
      0, provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL),
      0, 0));

  AddChildView(checkbox_);
  AddChildView(secondary_text_);
}

MediaGalleryCheckboxView::~MediaGalleryCheckboxView() {}

void MediaGalleryCheckboxView::Layout() {
  views::View::Layout();
  if (GetPreferredSize().width() <= GetLocalBounds().width())
    return;

  // If box layout doesn't fit, do custom layout. The secondary text should take
  // up at most half of the space and the checkbox can take up whatever is left.
  int checkbox_width = checkbox_->GetPreferredSize().width();
  int secondary_text_width = secondary_text_->GetPreferredSize().width();
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
