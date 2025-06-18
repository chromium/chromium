// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"

#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
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
constexpr int kIconSize = 24;
constexpr int kAnimationDurationMs = 450;

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsDropTargetView,
                                      kMultiContentsDropTargetElementId);

MultiContentsDropTargetView::MultiContentsDropTargetView(
    DropDelegate& drop_delegate)
    : views::AnimationDelegateViews(this), drop_delegate_(drop_delegate) {
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

  SetBackground(views::CreateSolidBackground(ui::kColorPrimaryBackground));

  auto inner_container = std::make_unique<views::View>();

  inner_container->SetBackground(views::CreateRoundedRectBackground(
      ui::kColorSysSurface3, kInnerCornerRadius));

  inner_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(
          gfx::Insets(features::kSideBySideDropTargetInnerPadding.Get()))
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

int MultiContentsDropTargetView::GetPreferredWidth() const {
  if (!GetVisible()) {
    return 0;
  }
  return GetAnimationValue() * GetPreferredSize().width();
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

void MultiContentsDropTargetView::Show(DropSide side) {
  side_ = side;
  UpdateVisibility(true);
}

void MultiContentsDropTargetView::Hide() {
  UpdateVisibility(false);
}

void MultiContentsDropTargetView::SetVisible(bool visible) {
  if (!visible) {
    side_.reset();
  }
  views::View::SetVisible(visible);
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

bool MultiContentsDropTargetView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::URL;
  format_types->insert(ui::ClipboardFormatType::UrlType());
  return true;
}

// Allows dropping links only.
bool MultiContentsDropTargetView::CanDrop(const OSExchangeData& data) {
  if (!data.HasURL(ui::FilenameToURLPolicy::CONVERT_FILENAMES)) {
    return false;
  }
  auto urls = data.GetURLs(ui::FilenameToURLPolicy::CONVERT_FILENAMES);
  return urls.has_value() && !urls.value().empty();
}

int MultiContentsDropTargetView::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_LINK;
}

void MultiContentsDropTargetView::OnDragExited() {
  Hide();
}

void MultiContentsDropTargetView::OnDragDone() {
  Hide();
}

views::View::DropCallback MultiContentsDropTargetView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  return base::BindOnce(&MultiContentsDropTargetView::DoDrop,
                        base::Unretained(this));
}

void MultiContentsDropTargetView::DoDrop(
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  CHECK(side_.has_value());
  DropSide side = side_.value();
  Hide();
  auto urls = event.data().GetURLs(ui::FilenameToURLPolicy::CONVERT_FILENAMES);
  CHECK(urls.has_value());
  drop_delegate_->HandleLinkDrop(side, urls.value());
}

BEGIN_METADATA(MultiContentsDropTargetView)
END_METADATA
