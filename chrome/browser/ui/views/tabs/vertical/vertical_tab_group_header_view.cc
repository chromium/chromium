// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"

#include <numeric>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_tracker.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kGroupHeaderCornerRadius = 8;
constexpr int kGroupHeaderHorizontalInset = 8;
constexpr int kIconSize = 16;

class VerticalTabGroupHeaderLabel : public views::Label {
  METADATA_HEADER(VerticalTabGroupHeaderLabel, views::Label)
 public:
  VerticalTabGroupHeaderLabel() {
    SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    SetElideBehavior(gfx::FADE_TAIL);
    SetAutoColorReadabilityEnabled(false);
  }
};

BEGIN_METADATA(VerticalTabGroupHeaderLabel)
END_METADATA
}  // namespace

VerticalTabGroupHeaderView::VerticalTabGroupHeaderView(
    Delegate* delegate,
    const tab_groups::TabGroupVisualData* tab_group_visual_data)
    : group_header_label_(
          AddChildView(std::make_unique<VerticalTabGroupHeaderLabel>())),
      collapse_icon_(AddChildView(std::make_unique<views::ImageView>())),
      delegate_(delegate) {
  SetProperty(views::kElementIdentifierKey, kTabGroupHeaderElementId);

  SetInteriorMargin(gfx::Insets::VH(0, kGroupHeaderHorizontalInset));
  group_header_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
          views::MaximumFlexSizeRule::kUnbounded));

  OnDataChanged(tab_group_visual_data);
}

VerticalTabGroupHeaderView::~VerticalTabGroupHeaderView() = default;

bool VerticalTabGroupHeaderView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    delegate_->ToggleCollapsedState(
        ToggleTabGroupCollapsedStateOrigin::kKeyboard);
    return true;
  }
  return false;
}

bool VerticalTabGroupHeaderView::OnMousePressed(const ui::MouseEvent& event) {
  // Ignore the click if the editor is already open. Do this so clicking
  // on us again doesn't re-trigger the editor.
  //
  // Though the bubble is deactivated before we receive a mouse event,
  // the actual widget destruction happens in a posted task. That task
  // gets run after we receive the mouse event. If this sounds brittle,
  // that's because it is!
  if (editor_bubble_tracker_.is_open()) {
    return false;
  }

  // Return true so that we receive subsequent MouseRelease event.
  return true;
}

void VerticalTabGroupHeaderView::OnMouseReleased(const ui::MouseEvent& event) {
  bool open_editor_bubble =
      event.IsRightMouseButton() && !editor_bubble_tracker_.is_open();
  bool toggle_collapse = event.IsLeftMouseButton();
  if (open_editor_bubble) {
    editor_bubble_tracker_.Opened(delegate_->ShowGroupEditorBubble(
        /*stop_context_menu_propagation=*/false));
  } else if (toggle_collapse) {
    delegate_->ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMouse);
  }
}

void VerticalTabGroupHeaderView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureTap:
      delegate_->ToggleCollapsedState(
          ToggleTabGroupCollapsedStateOrigin::kGesture);
      break;
    case ui::EventType::kGestureLongTap:
      editor_bubble_tracker_.Opened(delegate_->ShowGroupEditorBubble(
          /*stop_context_menu_propagation=*/false));
      break;
    default:
      break;
  }
  event->SetHandled();
}

void VerticalTabGroupHeaderView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  // When the context menu is triggered via keyboard, the keyboard event
  // propagates to the textfield inside the Editor Bubble. In those cases, we
  // want to tell the Editor Bubble to stop the event by setting
  // stop_context_menu_propagation to true.
  //
  // However, when the context menu is triggered via mouse, the same event
  // sequence doesn't happen. Stopping the context menu propagation in that case
  // would artificially hide the textfield's context menu the first time the
  // user tried to access it. So we don't want to stop the context menu
  // propagation if this call is reached via mouse.
  //
  // Notably, event behavior with a mouse is inconsistent depending on
  // OS. When not on Mac, the OnMouseReleased() event happens first and opens
  // the Editor Bubble early, preempting the Show() call below. On Mac, the
  // ShowContextMenu() event happens first and the Show() call is made here.
  //
  // So, because of the event order on non-Mac, and because there is no native
  // way to open a context menu via keyboard on Mac, we assume that we've
  // reached this function via mouse if and only if the current OS is Mac.
  // Therefore, we don't stop the menu propagation in that case.
  constexpr bool kStopContextMenuPropagation =
#if BUILDFLAG(IS_MAC)
      false;
#else
      true;
#endif

  editor_bubble_tracker_.Opened(
      delegate_->ShowGroupEditorBubble(kStopContextMenuPropagation));
}

void VerticalTabGroupHeaderView::OnDataChanged(
    const tab_groups::TabGroupVisualData* tab_group_visual_data) {
  group_header_label_->SetText(tab_group_visual_data->title());
  if (GetColorProvider()) {
    SkColor background_color = GetColorProvider()->GetColor(
        GetTabGroupTabStripColorId(tab_group_visual_data->color(),
                                   GetWidget()->ShouldPaintAsActive()));
    SkColor forground_color =
        color_utils::GetColorWithMaxContrast(background_color);
    group_header_label_->SetEnabledColor(forground_color);
    collapse_icon_->SetImage(
        ui::ImageModel::FromVectorIcon(tab_group_visual_data->is_collapsed()
                                           ? kKeyboardArrowDownChromeRefreshIcon
                                           : kKeyboardArrowUpChromeRefreshIcon,
                                       forground_color, kIconSize));
    SetBackground(views::CreateRoundedRectBackground(background_color,
                                                     kGroupHeaderCornerRadius));
  }
}

BEGIN_METADATA(VerticalTabGroupHeaderView)
END_METADATA
