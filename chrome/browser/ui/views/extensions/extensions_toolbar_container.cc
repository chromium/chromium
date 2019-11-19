// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "base/numerics/ranges.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/widget/widget_observer.h"

struct ExtensionsToolbarContainer::DropInfo {
  DropInfo(ToolbarActionsModel::ActionId action_id, size_t index);

  // The id for the action being dragged.
  ToolbarActionsModel::ActionId action_id;

  // The (0-indexed) icon before the action will be dropped.
  size_t index;
};

ExtensionsToolbarContainer::DropInfo::DropInfo(
    ToolbarActionsModel::ActionId action_id,
    size_t index)
    : action_id(action_id), index(index) {}

ExtensionsToolbarContainer::ExtensionsToolbarContainer(Browser* browser)
    : ToolbarIconContainerView(/*uses_highlight=*/true),
      browser_(browser),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      model_observer_(this),
      extensions_button_(new ExtensionsToolbarButton(browser_, this)) {
  model_observer_.Add(model_);
  // Do not flip the Extensions icon in RTL.
  extensions_button_->EnableCanvasFlippingForRTLUI(false);
  AddMainButton(extensions_button_);
  CreateActions();
}

ExtensionsToolbarContainer::~ExtensionsToolbarContainer() {
  if (active_bubble_)
    active_bubble_->GetWidget()->Close();
  // We should synchronously receive the OnWidgetClosing() event, so we should
  // always have cleared the active bubble by now.
  DCHECK(!active_bubble_);
}

void ExtensionsToolbarContainer::UpdateAllIcons() {
  extensions_button_->UpdateIcon();

  for (const auto& action : actions_)
    action->UpdateState();
}

ToolbarActionView* ExtensionsToolbarContainer::GetViewForId(
    const std::string& id) {
  auto it = icons_.find(id);
  if (it == icons_.end())
    return nullptr;
  return it->second.get();
}

ToolbarActionViewController* ExtensionsToolbarContainer::GetActionForId(
    const std::string& action_id) {
  for (const auto& action : actions_) {
    if (action->GetId() == action_id)
      return action.get();
  }
  return nullptr;
}

ToolbarActionViewController* ExtensionsToolbarContainer::GetPoppedOutAction()
    const {
  return popped_out_action_;
}

bool ExtensionsToolbarContainer::IsActionVisibleOnToolbar(
    const ToolbarActionViewController* action) const {
  return model_->IsActionPinned(action->GetId()) ||
         action == popped_out_action_ ||
         (active_bubble_ &&
          action->GetId() == active_bubble_->GetAnchorActionId());
}

void ExtensionsToolbarContainer::UndoPopOut() {
  DCHECK(popped_out_action_);
  ToolbarActionViewController* const popped_out_action = popped_out_action_;
  popped_out_action_ = nullptr;
  // Note that we only hide this view if it was not pinned while being popped
  // out.
  icons_[popped_out_action->GetId()]->SetVisible(
      IsActionVisibleOnToolbar(popped_out_action));
}

void ExtensionsToolbarContainer::SetPopupOwner(
    ToolbarActionViewController* popup_owner) {
  // We should never be setting a popup owner when one already exists, and
  // never unsetting one when one wasn't set.
  DCHECK((popup_owner_ != nullptr) ^ (popup_owner != nullptr));
  popup_owner_ = popup_owner;
}

void ExtensionsToolbarContainer::HideActivePopup() {
  if (popup_owner_)
    popup_owner_->HidePopup();
  DCHECK(!popup_owner_);
}

bool ExtensionsToolbarContainer::CloseOverflowMenuIfOpen() {
  if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
    return true;
  }
  return false;
}

void ExtensionsToolbarContainer::PopOutAction(
    ToolbarActionViewController* action,
    bool is_sticky,
    const base::Closure& closure) {
  // TODO(pbos): Highlight popout differently.
  DCHECK(!popped_out_action_);
  popped_out_action_ = action;
  icons_[popped_out_action_->GetId()]->SetVisible(true);
  ReorderViews();
  static_cast<views::AnimatingLayoutManager*>(GetLayoutManager())
      ->RunOrQueueAction(closure);
}

void ExtensionsToolbarContainer::ShowToolbarActionBubble(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> controller) {
  auto iter = icons_.find(controller->GetAnchorActionId());

  views::View* const anchor_view = iter != icons_.end()
                                       ? static_cast<View*>(iter->second.get())
                                       : extensions_button_;

  anchor_view->SetVisible(true);

  static_cast<views::AnimatingLayoutManager*>(GetLayoutManager())
      ->RunOrQueueAction(
          base::BindOnce(&ExtensionsToolbarContainer::ShowActiveBubble,
                         weak_ptr_factory_.GetWeakPtr(), anchor_view,
                         base::Passed(std::move(controller))));
}

void ExtensionsToolbarContainer::ShowToolbarActionBubbleAsync(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExtensionsToolbarContainer::ShowToolbarActionBubble,
                     weak_ptr_factory_.GetWeakPtr(), std::move(bubble)));
}

void ExtensionsToolbarContainer::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  CreateActionForId(action_id);
  ReorderViews();
}

void ExtensionsToolbarContainer::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  // TODO(pbos): Handle extension upgrades, see ToolbarActionsBar. Arguably this
  // could be handled inside the model and be invisible to the container when
  // permissions are unchanged.

  // Delete the icon first so it unregisters it from the action.
  icons_.erase(action_id);
  base::EraseIf(
      actions_,
      [action_id](const std::unique_ptr<ToolbarActionViewController>& item) {
        return item->GetId() == action_id;
      });
}

void ExtensionsToolbarContainer::OnToolbarActionMoved(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {}

void ExtensionsToolbarContainer::OnToolbarActionLoadFailed() {}

void ExtensionsToolbarContainer::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewController* action = GetActionForId(action_id);
  if (action)
    action->UpdateState();
}

void ExtensionsToolbarContainer::OnToolbarVisibleCountChanged() {}

void ExtensionsToolbarContainer::OnToolbarHighlightModeChanged(
    bool is_highlighting) {}

void ExtensionsToolbarContainer::OnToolbarModelInitialized() {
  CreateActions();
}

void ExtensionsToolbarContainer::OnToolbarPinnedActionsChanged() {
  for (auto& it : icons_)
    it.second->SetVisible(IsActionVisibleOnToolbar(GetActionForId(it.first)));
  ReorderViews();
}

void ExtensionsToolbarContainer::ReorderViews() {
  const auto& pinned_action_ids = model_->pinned_action_ids();
  for (size_t i = 0; i < pinned_action_ids.size(); ++i)
    ReorderChildView(icons_[pinned_action_ids[i]].get(), i);

  if (drop_info_.get())
    ReorderChildView(icons_[drop_info_->action_id].get(), drop_info_->index);

  // Popped out actions should be at the end.
  if (popped_out_action_)
    ReorderChildView(icons_[popped_out_action_->GetId()].get(), -1);

  // The extension button is always last.
  ReorderChildView(extensions_button_, -1);
}

void ExtensionsToolbarContainer::CreateActions() {
  DCHECK(icons_.empty());
  DCHECK(actions_.empty());

  // If the model isn't initialized, wait for it.
  if (!model_->actions_initialized())
    return;

  for (auto& action_id : model_->action_ids())
    CreateActionForId(action_id);

  ReorderViews();
}

void ExtensionsToolbarContainer::CreateActionForId(
    const ToolbarActionsModel::ActionId& action_id) {
  actions_.push_back(
      model_->CreateActionForId(browser_, this, false, action_id));

  auto icon = std::make_unique<ToolbarActionView>(actions_.back().get(), this);
  icon->set_owned_by_client();
  icon->SetVisible(IsActionVisibleOnToolbar(actions_.back().get()));
  icon->AddButtonObserver(this);
  icon->AddObserver(this);
  AddChildView(icon.get());

  icons_[action_id] = std::move(icon);
}

content::WebContents* ExtensionsToolbarContainer::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

bool ExtensionsToolbarContainer::ShownInsideMenu() const {
  return false;
}

void ExtensionsToolbarContainer::OnToolbarActionViewDragDone() {}

views::LabelButton* ExtensionsToolbarContainer::GetOverflowReferenceView()
    const {
  return extensions_button_;
}

gfx::Size ExtensionsToolbarContainer::GetToolbarActionSize() {
  gfx::Rect rect(gfx::Size(28, 28));
  rect.Inset(-GetLayoutInsets(TOOLBAR_ACTION_VIEW));
  return rect.size();
}

void ExtensionsToolbarContainer::WriteDragDataForView(
    View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {
  DCHECK(data);

  auto it = std::find_if(model_->pinned_action_ids().cbegin(),
                         model_->pinned_action_ids().cend(),
                         [this, sender](const std::string& action_id) {
                           return GetViewForId(action_id) == sender;
                         });
  DCHECK(it != model_->pinned_action_ids().cend());

  size_t index = it - model_->pinned_action_ids().cbegin();

  ToolbarActionView* extension_view = GetViewForId(*it);
  data->provider().SetDragImage(GetExtensionIcon(extension_view),
                                press_pt.OffsetFromOrigin());
  // Fill in the remaining info.
  BrowserActionDragData drag_data(extension_view->view_controller()->GetId(),
                                  index);
  drag_data.Write(browser_->profile(), data);
}

int ExtensionsToolbarContainer::GetDragOperationsForView(View* sender,
                                                         const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_MOVE;
}

bool ExtensionsToolbarContainer::CanStartDragForView(View* sender,
                                                     const gfx::Point& press_pt,
                                                     const gfx::Point& p) {
  return true;
}

bool ExtensionsToolbarContainer::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return BrowserActionDragData::GetDropFormats(format_types);
}

bool ExtensionsToolbarContainer::AreDropTypesRequired() {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool ExtensionsToolbarContainer::CanDrop(const OSExchangeData& data) {
  return BrowserActionDragData::CanDrop(data, browser_->profile());
}

int ExtensionsToolbarContainer::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data()))
    return ui::DragDropTypes::DRAG_NONE;
  size_t before_icon = 0;
  // Figure out where to display the icon during dragging transition.

  // First, since we want to update the dragged extension's position from before
  // an icon to after it when the event passes the midpoint between two icons.
  // This will convert the event coordinate into the index of the icon we want
  // to display the dragged extension before. We also mirror the event.x() so
  // that our calculations are consistent with left-to-right.
  const int offset_into_icon_area = GetMirroredXInView(event.x());
  const int before_icon_unclamped = WidthToIconCount(offset_into_icon_area);

  int visible_icons = model_->pinned_action_ids().size();

  // Because the user can drag outside the container bounds, we need to clamp
  // to the valid range. Note that the maximum allowable value is
  // |visible_icons|, not (|visible_icons| - 1), because we represent the
  // dragged extension being past the last icon as being "before the (last + 1)
  // icon".
  before_icon = base::ClampToRange(before_icon_unclamped, 0, visible_icons);

  if (!drop_info_.get() || drop_info_->index != before_icon) {
    drop_info_ = std::make_unique<DropInfo>(data.id(), before_icon);
    SetExtensionIconVisibility(drop_info_->action_id, false);
    ReorderViews();
  }

  return ui::DragDropTypes::DRAG_MOVE;
}

void ExtensionsToolbarContainer::OnDragExited() {
  const ToolbarActionsModel::ActionId dragged_extension_id =
      drop_info_->action_id;
  drop_info_.reset();
  ReorderViews();
  static_cast<views::AnimatingLayoutManager*>(GetLayoutManager())
      ->RunOrQueueAction(base::BindOnce(
          &ExtensionsToolbarContainer::SetExtensionIconVisibility,
          weak_ptr_factory_.GetWeakPtr(), dragged_extension_id, true));
}

int ExtensionsToolbarContainer::OnPerformDrop(
    const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data()))
    return ui::DragDropTypes::DRAG_NONE;

  model_->MovePinnedAction(drop_info_->action_id, drop_info_->index);

  OnDragExited();  // Perform clean up after dragging.
  return ui::DragDropTypes::DRAG_MOVE;
}

void ExtensionsToolbarContainer::OnWidgetClosing(views::Widget* widget) {
  ClearActiveBubble(widget);
}

void ExtensionsToolbarContainer::OnWidgetDestroying(views::Widget* widget) {
  ClearActiveBubble(widget);
}

void ExtensionsToolbarContainer::ClearActiveBubble(views::Widget* widget) {
  DCHECK(active_bubble_);
  DCHECK_EQ(active_bubble_->GetWidget(), widget);
  ToolbarActionViewController* const action =
      GetActionForId(active_bubble_->GetAnchorActionId());
  // TODO(pbos): Note that this crashes if a bubble anchors to the menu and not
  // to an extension that gets popped out. This should be fixed, but a test
  // should first be added to make sure that it's covered.
  CHECK(action);
  active_bubble_ = nullptr;
  widget->RemoveObserver(this);
  // Note that we only hide this view if it's not visible for other reasons
  // than displaying the bubble.
  icons_[action->GetId()]->SetVisible(IsActionVisibleOnToolbar(action));
}

size_t ExtensionsToolbarContainer::WidthToIconCount(int x_offset) {
  const int element_padding = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  size_t unclamped_count =
      std::max((x_offset + element_padding) /
                   (GetToolbarActionSize().width() + element_padding),
               0);
  return std::min(unclamped_count, actions_.size());
}

gfx::ImageSkia ExtensionsToolbarContainer::GetExtensionIcon(
    ToolbarActionView* extension_view) {
  return extension_view->view_controller()
      ->GetIcon(GetCurrentWebContents(), GetToolbarActionSize())
      .AsImageSkia();
}

void ExtensionsToolbarContainer::SetExtensionIconVisibility(
    ToolbarActionsModel::ActionId id,
    bool visible) {
  auto it = std::find_if(model_->pinned_action_ids().cbegin(),
                         model_->pinned_action_ids().cend(),
                         [this, id](const std::string& action_id) {
                           return GetViewForId(action_id) == GetViewForId(id);
                         });
  ToolbarActionView* extension_view = GetViewForId(*it);
  extension_view->SetImage(
      views::Button::STATE_NORMAL,
      visible ? GetExtensionIcon(extension_view) : gfx::ImageSkia());
}

void ExtensionsToolbarContainer::ShowActiveBubble(
    views::View* anchor_view,
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> controller) {
  active_bubble_ = new ToolbarActionsBarBubbleViews(
      anchor_view, anchor_view != extensions_button_, std::move(controller));
  views::BubbleDialogDelegateView::CreateBubble(active_bubble_)
      ->AddObserver(this);
  active_bubble_->Show();
}
