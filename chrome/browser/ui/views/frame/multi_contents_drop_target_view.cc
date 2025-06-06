// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"

#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_type.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_constants.h"
#include "ui/views/view_utils.h"

namespace {

constexpr float kInnerCornerRadius = 6;
constexpr int kOuterPadding = 8;
constexpr int kInnerPadding = 8;
constexpr int kIconSize = 24;
constexpr int kAnimationDurationMs = 450;

}  // namespace

MultiContentsDropTargetView::MultiContentsDropTargetView()
    : views::AnimationDelegateViews(this) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(gfx::Insets(kOuterPadding))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded));

  SetBackground(views::CreateSolidBackground(ui::kColorPrimaryBackground));

  auto inner_container = std::make_unique<views::View>();

  inner_container->SetBackground(views::CreateRoundedRectBackground(
      ui::kColorSysSurface3, kInnerCornerRadius));

  inner_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(gfx::Insets(kInnerPadding))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded));

  icon_view_ =
      inner_container->AddChildView(std::make_unique<views::ImageView>());
  inner_container_ = AddChildView(std::move(inner_container));

  animation_.SetTweenType(gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED);
  animation_.SetSlideDuration(base::Milliseconds(kAnimationDurationMs));
}

MultiContentsDropTargetView::~MultiContentsDropTargetView() = default;

double MultiContentsDropTargetView::GetAnimationValue() const {
  if (ShouldShowAnimation()) {
    return animation_.GetCurrentValue();
  }
  return 1;
}

bool MultiContentsDropTargetView::IsClosing() const {
  return animation_.IsClosing();
}

void MultiContentsDropTargetView::AnimationProgressed(
    const gfx::Animation* animation) {
  InvalidateLayout();
}

void MultiContentsDropTargetView::AnimationEnded(
    const gfx::Animation* animation) {
  if (animation->GetCurrentValue() == 0) {
    SetVisible(false);
  }
  InvalidateLayout();
}

void MultiContentsDropTargetView::Show() {
  UpdateVisibility(true);
}

void MultiContentsDropTargetView::Hide() {
  UpdateVisibility(false);
}

void MultiContentsDropTargetView::UpdateVisibility(bool should_be_open) {
  if (ShouldShowAnimation()) {
    if (should_be_open) {
      SetVisible(should_be_open);
      animation_.Show();
    } else if (GetVisible() && !IsClosing()) {
      animation_.Hide();
    }
  } else {
    animation_.Reset(should_be_open ? 1 : 0);
    SetVisible(should_be_open);
  }
}

bool MultiContentsDropTargetView::ShouldShowAnimation() const {
  return gfx::Animation::ShouldRenderRichAnimation();
}

void MultiContentsDropTargetView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SkColor primary_color = GetColorProvider()->GetColor(ui::kColorSysPrimary);
  ui::ImageModel icon_image_model =
      ui::ImageModel::FromVectorIcon(kAddCircleIcon, primary_color, kIconSize);
  icon_view_->SetImage(icon_image_model);
}

BEGIN_METADATA(MultiContentsDropTargetView)
END_METADATA
