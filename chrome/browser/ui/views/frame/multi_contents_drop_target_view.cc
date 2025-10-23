// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"

#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/layer_type.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_constants.h"
#include "ui/views/view_utils.h"

namespace {

constexpr float kInnerCornerRadius = 6;
constexpr int kOuterPadding = 8;
constexpr int kIconSize = 24;
constexpr int kAnimationDurationMs = 450;
constexpr gfx::Insets kInnerContainerMargins = gfx::Insets::VH(24, 10);

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsDropTargetView,
                                      kMultiContentsDropTargetElementId);

MultiContentsDropTargetView::MultiContentsDropTargetView()
    : views::AnimationDelegateViews(this) {
  SetVisible(false);
  SetProperty(views::kElementIdentifierKey, kMultiContentsDropTargetElementId);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(gfx::Insets(kOuterPadding))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded));

  auto inner_container = std::make_unique<views::View>();
  inner_container->SetBackground(views::CreateLayerBasedRoundedBackground(
      ui::kColorSysSurface3, gfx::RoundedCornersF(kInnerCornerRadius)));
  inner_container->background()->SetInternalName(
      "MultiContentsDropTargetView/InnerContainer");

  inner_container_layout_ =
      &inner_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
           ->SetOrientation(views::LayoutOrientation::kVertical)
           .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
           .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
           .SetInteriorMargin(kInnerContainerMargins)
           .SetDefault(
               views::kFlexBehaviorKey,
               views::FlexSpecification(
                   views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                   views::MaximumFlexSizeRule::kPreferred));

  icon_view_ =
      inner_container->AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  icon_view_->layer()->SetFillsBoundsOpaquely(false);
  icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
      kAddCircleIcon, ui::kColorSysPrimary, kIconSize));

  label_ = inner_container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SPLIT_VIEW_DRAG_ENTRYPOINT_LABEL)));
  label_->SetPaintToLayer(ui::LAYER_TEXTURED);
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->SetEnabledColor(ui::kColorSysPrimary);
  label_->SetElideBehavior(gfx::NO_ELIDE);
  label_->SetSubpixelRenderingEnabled(false);

  // This ensures that the height of the label is collapsed whenever its
  // width doesn't fit in the available space. This ensures the icon is
  // vertically centered whenever the label is collapsed.
  label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(base::BindRepeating(
          [](const views::View* view, const views::SizeBounds& bounds) {
            const auto preferred_size = view->GetPreferredSize();
            if (bounds.width().is_bounded() &&
                bounds.width().value() < preferred_size.width()) {
              return gfx::Size();
            }
            return view->GetPreferredSize(bounds);
          })));

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

void MultiContentsDropTargetView::SetDragDelegate(DragDelegate* drag_delegate) {
  drag_delegate_ = drag_delegate;
}

bool MultiContentsDropTargetView::IsClosing() const {
  return animation_.IsClosing();
}

// static
int MultiContentsDropTargetView::GetMaxWidth(int web_contents_width,
                                             DropTargetState state,
                                             DragType drag_type) {
  int min_width = 0;
  int max_width = 0;
  int percentage = 0;

  switch (state) {
    case DropTargetState::kNudge:
      CHECK(base::FeatureList::IsEnabled(features::kSideBySideDropTargetNudge));
      min_width = features::kSideBySideDropTargetNudgeMinWidth.Get();
      max_width = features::kSideBySideDropTargetNudgeMaxWidth.Get();
      percentage =
          features::kSideBySideDropTargetNudgeTargetWidthPercentage.Get();
      break;
    case DropTargetState::kNudgeToFull:
      CHECK(base::FeatureList::IsEnabled(features::kSideBySideDropTargetNudge));
      min_width = features::kSideBySideDropTargetNudgeToFullMinWidth.Get();
      max_width = features::kSideBySideDropTargetNudgeToFullMaxWidth.Get();
      percentage =
          features::kSideBySideDropTargetNudgeToFullTargetWidthPercentage.Get();
      break;
    case DropTargetState::kFull:
      min_width = features::kSideBySideDropTargetMinWidth.Get();
      max_width = features::kSideBySideDropTargetMaxWidth.Get();
      percentage =
          drag_type == DragType::kTab
              ? features::kSideBySideDropTargetTargetWidthPercentage.Get()
              : features::kSideBySideDropTargetForLinkTargetWidthPercentage
                    .Get();
      break;
    default:
      NOTREACHED();
  }

  // Calculate the target width based on the web contents width and the target
  // percentage.
  const int target_width = web_contents_width * (percentage / 100.0);

  // Clamp the width to the min and max widths.
  return std::clamp(target_width, min_width, max_width);
}

int MultiContentsDropTargetView::GetPreferredWidth(
    int web_contents_width) const {
  if (!GetVisible()) {
    return 0;
  }

  CHECK(state_.has_value());
  CHECK(drag_type_.has_value());

  const int target_full_width =
      GetMaxWidth(web_contents_width, *state_, drag_type_.value());
  const int animation_start_width = animate_expand_starting_width_.value_or(0);
  return animation_start_width +
         (GetAnimationValue() * (target_full_width - animation_start_width));
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

void MultiContentsDropTargetView::Show(DropSide side,
                                       DropTargetState state,
                                       DragType drag_type) {
  if (state == DropTargetState::kNudge ||
      state == DropTargetState::kNudgeToFull) {
    CHECK(base::FeatureList::IsEnabled(features::kSideBySideDropTargetNudge));
  }

  // If transitioning from a nudge to full state, start a new animation.
  if (state == DropTargetState::kNudgeToFull &&
      state_ == MultiContentsDropTargetView::DropTargetState::kNudge) {
    animation_.Reset(0);
  }

  label_->SetVisible(state != DropTargetState::kNudge);

  side_ = side;
  state_ = state;
  drag_type_ = drag_type;

  inner_container_layout_->SetMainAxisAlignment(
      drag_type_ == DragType::kTab ? views::LayoutAlignment::kStart
                                   : views::LayoutAlignment::kCenter);

  UpdateVisibility(true);
}

void MultiContentsDropTargetView::Hide(bool suppress_animation /*=false*/) {
  base::AutoReset<bool> auto_reset(&should_suppress_animation_,
                                   suppress_animation);
  UpdateVisibility(false);
}

void MultiContentsDropTargetView::SetVisible(bool visible) {
  if (!visible) {
    side_.reset();
    drag_type_.reset();
  }
  views::View::SetVisible(visible);
}

void MultiContentsDropTargetView::UpdateVisibility(bool should_be_open) {
  if (!should_be_open || !GetVisible()) {
    animate_expand_starting_width_.reset();
  } else if (animation_.GetCurrentValue() == 0) {
    // If starting a new "expand" animation, then update the starting width.
    animate_expand_starting_width_ = width();
  }
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
    InvalidateLayout();
  }
}

bool MultiContentsDropTargetView::ShouldShowAnimation() const {
  return gfx::Animation::ShouldRenderRichAnimation() &&
         !gfx::Animation::PrefersReducedMotion() && !should_suppress_animation_;
}

bool MultiContentsDropTargetView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  CHECK(drag_delegate_);
  return drag_delegate_->GetDropFormats(formats, format_types);
}

// Allows dropping links only.
bool MultiContentsDropTargetView::CanDrop(const OSExchangeData& data) {
  CHECK(drag_delegate_);
  return drag_delegate_->CanDrop(data);
}

void MultiContentsDropTargetView::OnDragEntered(
    const ui::DropTargetEvent& event) {
  CHECK(drag_delegate_);
  drag_delegate_->OnDragEntered(event);
}

int MultiContentsDropTargetView::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  CHECK(drag_delegate_);
  return drag_delegate_->OnDragUpdated(event);
}

void MultiContentsDropTargetView::OnDragExited() {
  CHECK(drag_delegate_);
  drag_delegate_->OnDragExited();
}

void MultiContentsDropTargetView::OnDragDone() {
  CHECK(drag_delegate_);
  drag_delegate_->OnDragDone();
}

views::View::DropCallback MultiContentsDropTargetView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  CHECK(drag_delegate_);
  return drag_delegate_->GetDropCallback(event);
}

BEGIN_METADATA(MultiContentsDropTargetView)
END_METADATA
