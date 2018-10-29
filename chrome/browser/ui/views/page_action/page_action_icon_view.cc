// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/platform_style.h"

namespace {

bool ActivateButtonOnSpaceDown() {
  return views::PlatformStyle::kKeyClickActionOnSpace ==
         views::Button::KeyClickAction::CLICK_ON_KEY_PRESS;
}

}  // namespace

void PageActionIconView::Init() {
  AddChildView(image());
  image()->set_can_process_events_within_subtree(false);
  image()->EnableCanvasFlippingForRTLUI(true);
  SetInkDropMode(InkDropMode::ON);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

PageActionIconView::PageActionIconView(CommandUpdater* command_updater,
                                       int command_id,
                                       PageActionIconView::Delegate* delegate,
                                       const gfx::FontList& font_list)
    : IconLabelBubbleView(font_list),
      icon_size_(GetLayoutConstant(LOCATION_BAR_ICON_SIZE)),
      command_updater_(command_updater),
      delegate_(delegate),
      command_id_(command_id),
      active_(false),
      suppress_mouse_released_action_(false) {
  set_ink_drop_visible_opacity(
      GetOmniboxStateAlpha(OmniboxPartState::SELECTED));
}

PageActionIconView::~PageActionIconView() {}

bool PageActionIconView::IsBubbleShowing() const {
  // If the bubble is being destroyed, it's considered showing though it may be
  // already invisible currently.
  return GetBubble() != nullptr;
}

bool PageActionIconView::SetCommandEnabled(bool enabled) const {
  DCHECK(command_updater_);
  command_updater_->UpdateCommandEnabled(command_id_, enabled);
  return command_updater_->IsCommandEnabled(command_id_);
}

bool PageActionIconView::Update() {
  return false;
}

SkColor PageActionIconView::GetTextColor() const {
  // Returns the color of the label shown during animation.
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelDisabledColor);
}

void PageActionIconView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetName(GetTextForTooltipAndAccessibleName());
}

bool PageActionIconView::GetTooltipText(const gfx::Point& p,
                                        base::string16* tooltip) const {
  if (IsBubbleShowing())
    return false;
  *tooltip = GetTextForTooltipAndAccessibleName();
  return true;
}

bool PageActionIconView::OnMousePressed(const ui::MouseEvent& event) {
  // If the bubble is showing then don't reshow it when the mouse is released.
  suppress_mouse_released_action_ = IsBubbleShowing();
  if (!suppress_mouse_released_action_ && event.IsOnlyLeftMouseButton())
    AnimateInkDrop(views::InkDropState::ACTION_PENDING, &event);

  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void PageActionIconView::OnMouseReleased(const ui::MouseEvent& event) {
  // If this is the second click on this view then the bubble was showing on the
  // mouse pressed event and is hidden now. Prevent the bubble from reshowing by
  // doing nothing here.
  if (suppress_mouse_released_action_) {
    suppress_mouse_released_action_ = false;
    OnPressed(false);
    return;
  }
  if (!event.IsLeftMouseButton())
    return;

  const bool activated = HitTestPoint(event.location());
  AnimateInkDrop(
      activated ? views::InkDropState::ACTIVATED : views::InkDropState::HIDDEN,
      &event);
  if (activated)
    ExecuteCommand(EXECUTE_SOURCE_MOUSE);
  OnPressed(activated);
}

bool PageActionIconView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_RETURN && event.key_code() != ui::VKEY_SPACE)
    return false;

  AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr /* &event */);
  // This behavior is duplicated from Button: on some platforms buttons activate
  // on VKEY_SPACE keydown, and on some platforms they activate on VKEY_SPACE
  // keyup. All platforms activate buttons on VKEY_RETURN keydown though.
  if (ActivateButtonOnSpaceDown() || event.key_code() == ui::VKEY_RETURN)
    ExecuteCommand(EXECUTE_SOURCE_KEYBOARD);
  return true;
}

bool PageActionIconView::OnKeyReleased(const ui::KeyEvent& event) {
  // If buttons activate on VKEY_SPACE keydown, don't re-execute the command on
  // keyup.
  if (event.key_code() != ui::VKEY_SPACE || ActivateButtonOnSpaceDown())
    return false;

  ExecuteCommand(EXECUTE_SOURCE_KEYBOARD);
  return true;
}

void PageActionIconView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);
  if (details.is_add && details.child == this && GetNativeTheme())
    UpdateIconImage();
}

void PageActionIconView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateIconImage();
}

void PageActionIconView::OnThemeChanged() {
  UpdateIconImage();
}

void PageActionIconView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  image()->SetPaintToLayer();
  image()->layer()->SetFillsBoundsOpaquely(false);
  IconLabelBubbleView::AddInkDropLayer(ink_drop_layer);
}

void PageActionIconView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  IconLabelBubbleView::RemoveInkDropLayer(ink_drop_layer);
  image()->DestroyLayer();
}

std::unique_ptr<views::InkDrop> PageActionIconView::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnFocus(!focus_ring());
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> PageActionIconView::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight>
PageActionIconView::CreateInkDropHighlight() const {
  std::unique_ptr<views::InkDropHighlight> highlight =
      CreateDefaultInkDropHighlight(
          gfx::RectF(GetMirroredRect(GetContentsBounds())).CenterPoint(),
          size());
  highlight->set_visible_opacity(
      GetOmniboxStateAlpha(OmniboxPartState::HOVERED));
  return highlight;
}

std::unique_ptr<views::InkDropMask> PageActionIconView::CreateInkDropMask()
    const {
  return std::make_unique<views::RoundRectInkDropMask>(size(), gfx::Insets(),
                                                       height() / 2.f);
}

SkColor PageActionIconView::GetInkDropBaseColor() const {
  return delegate_->GetPageActionInkDropColor();
}

void PageActionIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    AnimateInkDrop(views::InkDropState::ACTIVATED, event);
    ExecuteCommand(EXECUTE_SOURCE_GESTURE);
    event->SetHandled();
  }
}

void PageActionIconView::ExecuteCommand(ExecuteSource source) {
  OnExecuting(source);
  if (command_updater_)
    command_updater_->ExecuteCommand(command_id_);
}

void PageActionIconView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::BubbleDialogDelegateView* bubble = GetBubble();
  if (bubble)
    bubble->OnAnchorBoundsChanged();
  IconLabelBubbleView::OnBoundsChanged(previous_bounds);
}

void PageActionIconView::OnTouchUiChanged() {
  icon_size_ = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
  UpdateIconImage();
  IconLabelBubbleView::OnTouchUiChanged();
}

void PageActionIconView::UpdateBorder() {
  SetBorder(views::CreateEmptyBorder(
      GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING)));
}

void PageActionIconView::SetIconColor(SkColor icon_color) {
  icon_color_ = icon_color;
  UpdateIconImage();
}

void PageActionIconView::UpdateIconImage() {
  const ui::NativeTheme* theme = GetNativeTheme();
  SkColor icon_color = active_
                           ? theme->GetSystemColor(
                                 ui::NativeTheme::kColorId_ProminentButtonColor)
                           : icon_color_;
  SetImage(gfx::CreateVectorIcon(GetVectorIcon(), icon_size_, icon_color));
}

void PageActionIconView::SetActiveInternal(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  UpdateIconImage();
}

content::WebContents* PageActionIconView::GetWebContents() const {
  return delegate_->GetWebContentsForPageActionIconView();
}
