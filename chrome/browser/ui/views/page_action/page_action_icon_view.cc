// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

#include <utility>

#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_loading_indicator_view.h"
#include "ui/accessibility/ax_enums.mojom.h"
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
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/platform_style.h"

float PageActionIconView::Delegate::GetPageActionInkDropVisibleOpacity() const {
  return GetOmniboxStateOpacity(OmniboxPartState::SELECTED);
}

int PageActionIconView::Delegate::GetPageActionIconSize() const {
  return GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
}

gfx::Insets PageActionIconView::Delegate::GetPageActionIconInsets(
    const PageActionIconView* icon_view) const {
  return GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING);
}

bool PageActionIconView::Delegate::ShouldHidePageActionIcons() const {
  return false;
}

const OmniboxView* PageActionIconView::Delegate::GetOmniboxView() const {
  // Should not reach here: should call subclass's implementation.
  NOTREACHED();
  return nullptr;
}

PageActionIconView::PageActionIconView(
    CommandUpdater* command_updater,
    int command_id,
    IconLabelBubbleView::Delegate* parent_delegate,
    PageActionIconView::Delegate* delegate,
    const gfx::FontList& font_list)
    : IconLabelBubbleView(font_list, parent_delegate),
      command_updater_(command_updater),
      delegate_(delegate),
      command_id_(command_id) {
  DCHECK(delegate_);

  image()->EnableCanvasFlippingForRTLUI(true);
  SetInkDropMode(InkDropMode::ON);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  // Only shows bubble after mouse is released.
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnRelease);
  UpdateBorder();
}

PageActionIconView::~PageActionIconView() = default;

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

SkColor PageActionIconView::GetLabelColorForTesting() const {
  return label()->GetEnabledColor();
}

void PageActionIconView::ExecuteForTesting() {
  DCHECK(GetVisible());
  OnExecuting(EXECUTE_SOURCE_MOUSE);
}

void PageActionIconView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  const base::string16 name_text = GetTextForTooltipAndAccessibleName();
  node_data->SetName(name_text);
}

base::string16 PageActionIconView::GetTooltipText(const gfx::Point& p) const {
  return IsBubbleShowing() ? base::string16()
                           : GetTextForTooltipAndAccessibleName();
}

void PageActionIconView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);
  if (details.is_add && details.child == this && GetNativeTheme()) {
    UpdateIconImage();
    UpdateBorder();
  }
}

void PageActionIconView::OnThemeChanged() {
  IconLabelBubbleView::OnThemeChanged();
  UpdateIconImage();
}

bool PageActionIconView::ShouldShowSeparator() const {
  return false;
}

void PageActionIconView::NotifyClick(const ui::Event& event) {
  // Intentionally skip the immediate parent function
  // IconLabelBubbleView::NotifyClick(). It calls ShowBubble() which
  // is redundant here since we use Chrome command to show the bubble.
  LabelButton::NotifyClick(event);
  ExecuteSource source;
  if (event.IsMouseEvent()) {
    source = EXECUTE_SOURCE_MOUSE;
  } else if (event.IsKeyEvent()) {
    source = EXECUTE_SOURCE_KEYBOARD;
  } else if (event.IsGestureEvent()) {
    source = EXECUTE_SOURCE_GESTURE;
  } else {
    NOTREACHED();
    return;
  }

  // Set ink drop state to ACTIVATED.
  SetHighlighted(true);
  ExecuteCommand(source);
}

bool PageActionIconView::IsTriggerableEvent(const ui::Event& event) {
  // For PageActionIconView, returns whether the bubble should be shown given
  // the event happened. For mouse event, only shows bubble when the bubble is
  // not visible and when event is a left button click.
  if (event.IsMouseEvent()) {
    // IconLabelBubbleView allows any mouse click to be triggerable event so
    // need to manually check here.
    return IconLabelBubbleView::IsTriggerableEvent(event) &&
           ((GetTriggerableEventFlags() & event.flags()) != 0);
  }

  return IconLabelBubbleView::IsTriggerableEvent(event);
}

bool PageActionIconView::ShouldUpdateInkDropOnClickCanceled() const {
  // Override IconLabelBubbleView since for PageActionIconView if click is
  // cancelled due to bubble being visible, the InkDropState is ACTIVATED. So
  // the ink drop will not be updated anyway. Setting this to true will help to
  // update ink drop in other cases where clicks are cancelled.
  return true;
}

void PageActionIconView::ExecuteCommand(ExecuteSource source) {
  OnExecuting(source);
  if (command_updater_)
    command_updater_->ExecuteCommand(command_id_);
}

const gfx::VectorIcon& PageActionIconView::GetVectorIconBadge() const {
  return gfx::kNoneIcon;
}

void PageActionIconView::OnTouchUiChanged() {
  UpdateIconImage();
  IconLabelBubbleView::OnTouchUiChanged();
}

const char* PageActionIconView::GetClassName() const {
  return "PageActionIconView";
}

void PageActionIconView::SetIconColor(SkColor icon_color) {
  icon_color_ = icon_color;
  UpdateIconImage();
}

void PageActionIconView::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  UpdateIconImage();
}

void PageActionIconView::Update() {
  // Currently no page action icon should be visible during user input.
  // A future subclass may need a hook here if that changes.
  if (delegate_->ShouldHidePageActionIcons()) {
    ResetSlideAnimation(/*show_label=*/false);
    SetVisible(false);
  } else {
    UpdateImpl();
  }
}

void PageActionIconView::UpdateIconImage() {
  const ui::NativeTheme* theme = GetNativeTheme();
  const SkColor icon_color =
      active_ ? theme->GetSystemColor(
                    ui::NativeTheme::kColorId_ProminentButtonColor)
              : icon_color_;
  const int icon_size = delegate_->GetPageActionIconSize();
  const gfx::ImageSkia image = gfx::CreateVectorIconWithBadge(
      GetVectorIcon(), icon_size, icon_color, GetVectorIconBadge());
  if (!image.isNull())
    SetImageModel(ui::ImageModel::FromImageSkia(image));
}

void PageActionIconView::InstallLoadingIndicator() {
  if (loading_indicator_)
    return;

  loading_indicator_ =
      AddChildView(std::make_unique<PageActionIconLoadingIndicatorView>(this));
  loading_indicator_->SetVisible(false);
}

void PageActionIconView::SetIsLoading(bool is_loading) {
  if (!loading_indicator_)
    return;

  is_loading ? loading_indicator_->ShowAnimation()
             : loading_indicator_->StopAnimation();
}

content::WebContents* PageActionIconView::GetWebContents() const {
  return delegate_->GetWebContentsForPageActionIconView();
}

void PageActionIconView::UpdateBorder() {
  const gfx::Insets new_insets = delegate_->GetPageActionIconInsets(this);
  if (new_insets != GetInsets())
    SetBorder(views::CreateEmptyBorder(new_insets));
}
