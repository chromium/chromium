// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_row_grouped_view.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/flex_layout.h"

OmniboxRowGroupedView::OmniboxRowGroupedView(OmniboxPopupViewViews* popup_view)
    : popup_view_(popup_view) {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());

  // The layout needs to set axis alignment to end so that the rows at the
  // bottom of the view animate in first.
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kEnd);

  // The view needs a layer to animate opacity.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  animation_ = std::make_unique<gfx::SlideAnimation>(this);
}

OmniboxRowGroupedView::~OmniboxRowGroupedView() = default;

int OmniboxRowGroupedView::GetCurrentHeight() const {
  CHECK(animation_);
  if (animation_->is_animating()) {
    return animation_->CurrentValueBetween(0, GetPreferredSize().height());
  }
  return GetVisible() ? GetPreferredSize().height() : 0;
}

void OmniboxRowGroupedView::OnPopupHide() {
  has_animated = false;
}

void OmniboxRowGroupedView::MaybeStartAnimation() {
  CHECK(animation_);
  if (has_animated || animation_->is_animating()) {
    return;
  }
  has_animated = true;
  layer()->SetOpacity(0.0f);
  layer()->SetTransform(gfx::Transform());
  animation_->Reset();
  animation_->SetTweenType(gfx::Tween::EASE_OUT_3);
  animation_->SetSlideDuration(
      base::Milliseconds(omnibox_feature_configs::ContextualSearch::Get()
                             .loading_suggestions_position_animation_duration));
  animation_->Show();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OmniboxRowGroupedView::AnimateOpacity,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(omnibox_feature_configs::ContextualSearch::Get()
                             .loading_suggestions_opacity_animation_delay));
}

void OmniboxRowGroupedView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation != animation_.get()) {
    return;
  }
  // The position animation start a standard omnibox row height above where the
  // view will eventually be positioned.
  const int y =
      animation->CurrentValueBetween(-OmniboxMatchCellView::kRowHeight, 0);
  gfx::Transform transform;
  transform.Translate(0, y);
  layer()->SetTransform(transform);
  popup_view_->UpdatePopupBounds();
}

void OmniboxRowGroupedView::AnimateOpacity() {
  // The view may be destroyed before the delayed task runs.
  if (!GetWidget()) {
    return;
  }
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::Milliseconds(omnibox_feature_configs::ContextualSearch::Get()
                             .loading_suggestions_opacity_animation_duration));
  settings.SetTweenType(gfx::Tween::LINEAR);
  layer()->SetOpacity(1.0f);
}

BEGIN_METADATA(OmniboxRowGroupedView)
END_METADATA
