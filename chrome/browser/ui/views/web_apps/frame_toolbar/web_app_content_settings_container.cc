// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_content_settings_container.h"

#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/window/custom_frame_view.h"
#include "ui/views/window/hit_test_utils.h"

namespace {

constexpr base::TimeDelta kContentSettingsFadeInDuration =
    base::TimeDelta::FromMilliseconds(500);

}  // namespace

WebAppContentSettingsContainer::WebAppContentSettingsContainer(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    ContentSettingImageView::Delegate* content_setting_image_delegate) {
  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          views::LayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), icon_label_bubble_delegate,
        content_setting_image_delegate,
        views::CustomFrameView::GetWindowTitleFontList());
    // Padding around content setting icons.
    constexpr auto kContentSettingIconInteriorPadding = gfx::Insets(4);
    image_view->SetBorder(
        views::CreateEmptyBorder(kContentSettingIconInteriorPadding));
    image_view->disable_animation();
    views::SetHitTestComponent(image_view.get(), static_cast<int>(HTCLIENT));
    content_setting_views_.push_back(image_view.get());
    AddChildView(image_view.release());
  }
}

WebAppContentSettingsContainer::~WebAppContentSettingsContainer() = default;

void WebAppContentSettingsContainer::UpdateContentSettingViewsVisibility() {
  for (auto* v : content_setting_views_)
    v->Update();
}

void WebAppContentSettingsContainer::SetIconColor(SkColor icon_color) {
  for (auto* v : content_setting_views_)
    v->SetIconColor(icon_color);
}

void WebAppContentSettingsContainer::SetUpForFadeIn() {
  SetVisible(false);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetOpacity(0);
}

void WebAppContentSettingsContainer::FadeIn() {
  if (GetVisible())
    return;
  SetVisible(true);
  DCHECK_EQ(layer()->opacity(), 0);
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(kContentSettingsFadeInDuration);
  layer()->SetOpacity(1);
}

void WebAppContentSettingsContainer::EnsureVisible() {
  SetVisible(true);
  if (layer())
    layer()->SetOpacity(1);
}

BEGIN_METADATA(WebAppContentSettingsContainer, views::View)
END_METADATA
