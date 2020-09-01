// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "base/numerics/ranges.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

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

ExtensionsToolbarContainer::ExtensionsToolbarContainer(Browser* browser,
                                                       DisplayMode display_mode)
    : ToolbarIconContainerView(/*uses_highlight=*/true),
      browser_(browser),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      model_observer_(this),
      extensions_button_(new ExtensionsToolbarButton(browser_, this)),
      display_mode_(display_mode) {
  // The container shouldn't show unless / until we have extensions available.
  SetVisible(false);

  model_observer_.Add(model_);
  // Do not flip the Extensions icon in RTL.
  extensions_button_->EnableCanvasFlippingForRTLUI(false);

  const views::FlexSpecification hide_icon_flex_specification =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithWeight(0);
  switch (display_mode) {
    case DisplayMode::kNormal:
      // In normal mode, the menu icon is always shown.
      extensions_button_->SetProperty(views::kFlexBehaviorKey,
                                      views::FlexSpecification());
      break;
    case DisplayMode::kCompact:
      // In compact mode, the menu icon can be hidden but has the highest
      // priority.
      extensions_button_->SetProperty(
          views::kFlexBehaviorKey, hide_icon_flex_specification.WithOrder(1));
      break;
  }
  extensions_button_->SetID(VIEW_ID_EXTENSIONS_MENU_BUTTON);
  AddMainButton(extensions_button_);
  target_layout_manager()
      ->SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse)
      .SetDefault(views::kFlexBehaviorKey,
                  hide_icon_flex_specification.WithOrder(3));
  CreateActions();

  // TODO(pbos): Consider splitting out tab-strip observing into another class.
  // Triggers for Extensions-related bubbles should preferably be separate from
  // the container where they are shown.
  browser_->tab_strip_model()->AddObserver(this);
}

ExtensionsToolbarContainer::~ExtensionsToolbarContainer() {
  // The child views hold pointers to the |actions_|, and thus need to be
  // destroyed before them.
  RemoveAllChildViews(true);

  // Create a copy of the anchored widgets, since |anchored_widgets_| will
  // be modified by closing them.
  std::vector<views::Widget*> widgets;
  widgets.reserve(anchored_widgets_.size());
  for (const auto& anchored_widget : anchored_widgets_)
    widgets.push_back(anchored_widget.widget);
  for (auto* widget : widgets)
    widget->Close();
  // The widgets should close synchronously (resulting in OnWidgetClosing()),
  // so |anchored_widgets_| should now be empty.
  DCHECK(anchored_widgets_.empty());
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void ExtensionsToolbarContainer::UpdateAllIcons() {
  extensions_button_->UpdateIcon();
  for (const auto& action : actions_)
    action->UpdateState();
}

ToolbarActionView* ExtensionsToolbarContainer::GetViewForId(
    const std::string& id) {
  const auto it = icons_.find(id);
  return (it == icons_.end()) ? nullptr : it->second;
}

void ExtensionsToolbarContainer::ShowWidgetForExtension(
    views::Widget* widget,
    const std::string& extension_id) {
  anchored_widgets_.push_back({widget, extension_id});
  widget->AddObserver(this);
  UpdateIconVisibility(extension_id);
  animating_layout_manager()->PostOrQueueAction(base::BindOnce(
      &ExtensionsToolbarContainer::AnchorAndShowWidgetImmediately,
      weak_ptr_factory_.GetWeakPtr(), widget));
}

views::Widget*
ExtensionsToolbarContainer::GetAnchoredWidgetForExtensionForTesting(
    const std::string& extension_id) {
  auto iter = std::find_if(anchored_widgets_.begin(), anchored_widgets_.end(),
                           [extension_id](const auto& info) {
                             return info.extension_id == extension_id;
                           });
  return iter->widget;
}

bool ExtensionsToolbarContainer::ShouldForceVisibility(
    const std::string& extension_id) const {
  if (popped_out_action_ && popped_out_action_->GetId() == extension_id)
    return true;

  if (extension_with_open_context_menu_id_.has_value() &&
      extension_with_open_context_menu_id_.value() == extension_id) {
    return true;
  }

  for (const auto& anchored_widget : anchored_widgets_) {
    if (anchored_widget.extension_id == extension_id)
      return true;
  }

  return false;
}

void ExtensionsToolbarContainer::UpdateIconVisibility(
    const std::string& extension_id) {
  ToolbarActionView* const action_view = GetViewForId(extension_id);
  if (!action_view)
    return;

  // Popped out action uses a flex rule that causes it to always be visible
  // regardless of space; default for actions is to drop out when there is
  // insufficient space. So if an action is being forced visible, it should have
  // a rule that gives it higher priority, and if it does not, it should use the
  // default.
  const bool must_show = ShouldForceVisibility(extension_id);
  if (must_show) {
    switch (display_mode_) {
      case DisplayMode::kNormal:
        // In normal mode, the icon's visibility is forced.
        action_view->SetProperty(views::kFlexBehaviorKey,
                                 views::FlexSpecification());
        break;
      case DisplayMode::kCompact:
        // In compact mode, the icon can still drop out, but receives precedence
        // over other actions.
        action_view->SetProperty(
            views::kFlexBehaviorKey,
            views::FlexSpecification(
                views::MinimumFlexSizeRule::kPreferredSnapToZero,
                views::MaximumFlexSizeRule::kPreferred)
                .WithWeight(0)
                .WithOrder(2));
        break;
    }
  } else {
    action_view->ClearProperty(views::kFlexBehaviorKey);
  }

  if (must_show ||
      (CanShowIconInToolbar() && model_->IsActionPinned(extension_id)))
    animating_layout_manager()->FadeIn(action_view);
  else
    animating_layout_manager()->FadeOut(action_view);
}

void ExtensionsToolbarContainer::AnchorAndShowWidgetImmediately(
    views::Widget* widget) {
  auto iter = std::find_if(
      anchored_widgets_.begin(), anchored_widgets_.end(),
      [widget](const auto& info) { return info.widget == widget; });

  if (iter == anchored_widgets_.end()) {
    // This should mean that the Widget destructed before we got to showing it.
    // |widget| is invalid here and should not be shown.
    return;
  }

  // TODO(pbos): Make extension removal close associated widgets. Right now, it
  // seems possible that:
  // * ShowWidgetForExtension starts
  // * Extension gets removed
  // * AnchorAndShowWidgetImmediately runs.
  // Revisit how to handle that, likely the Widget should Close on removal which
  // would remove the AnchoredWidget entry.

  views::View* const anchor_view = GetViewForId(iter->extension_id);
  widget->widget_delegate()->AsBubbleDialogDelegate()->SetAnchorView(
      anchor_view && anchor_view->GetVisible() ? anchor_view
                                               : extensions_button_);
  widget->Show();
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

void ExtensionsToolbarContainer::OnContextMenuShown(
    ToolbarActionViewController* extension) {
  // Only update the extension's toolbar visibility if the context menu is being
  // shown from an extension visible in the toolbar.
  if (!ExtensionsMenuView::IsShowing()) {
#if defined(OS_MAC)
    // TODO(crbug/1065584): Remove hiding active popup here once this bug is
    // fixed.
    HideActivePopup();
#endif
    extension_with_open_context_menu_id_ = extension->GetId();
    UpdateIconVisibility(extension_with_open_context_menu_id_.value());
  }
}

void ExtensionsToolbarContainer::OnContextMenuClosed(
    ToolbarActionViewController* extension) {
  // |extension_with_open_context_menu_id_| does not have a value when a context
  // menu is being shown from within the extensions menu.
  if (extension_with_open_context_menu_id_.has_value()) {
    base::Optional<extensions::ExtensionId> const
        extension_with_open_context_menu = extension_with_open_context_menu_id_;
    extension_with_open_context_menu_id_.reset();
    UpdateIconVisibility(extension_with_open_context_menu.value());
  }
}

bool ExtensionsToolbarContainer::IsActionVisibleOnToolbar(
    const ToolbarActionViewController* action) const {
  const std::string& extension_id = action->GetId();
  return ShouldForceVisibility(extension_id) ||
         model_->IsActionPinned(extension_id);
}

extensions::ExtensionContextMenuModel::ButtonVisibility
ExtensionsToolbarContainer::GetActionVisibility(
    const ToolbarActionViewController* action) const {
  extensions::ExtensionContextMenuModel::ButtonVisibility visibility =
      extensions::ExtensionContextMenuModel::PINNED;

  if (ShouldForceVisibility(action->GetId()) &&
      !model_->IsActionPinned(action->GetId())) {
    visibility = extensions::ExtensionContextMenuModel::TRANSITIVELY_VISIBLE;
  } else if (!IsActionVisibleOnToolbar(action)) {
    visibility = extensions::ExtensionContextMenuModel::UNPINNED;
  }
  return visibility;
}

void ExtensionsToolbarContainer::UndoPopOut() {
  DCHECK(popped_out_action_);
  ToolbarActionViewController* const popped_out_action = popped_out_action_;
  popped_out_action_ = nullptr;
  UpdateIconVisibility(popped_out_action->GetId());
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
  UpdateIconVisibility(action->GetId());
  animating_layout_manager()->PostOrQueueAction(closure);
}

bool ExtensionsToolbarContainer::ShowToolbarActionPopupForAPICall(
    const std::string& action_id) {
  // Don't override another popup, and only show in the active window.
  if (popped_out_action_ || !browser_->window()->IsActive())
    return false;

  ToolbarActionViewController* action = GetActionForId(action_id);
  // Since this was triggered by an API call, we never want to grant activeTab
  // to the extension.
  constexpr bool kGrantActiveTab = false;
  return action && action->ExecuteAction(
                       kGrantActiveTab,
                       ToolbarActionViewController::InvocationSource::kApi);
}

void ExtensionsToolbarContainer::ShowToolbarActionBubble(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> controller) {
  const std::string extension_id = controller->GetAnchorActionId();

  views::View* const anchor_view = GetViewForId(extension_id);

  views::Widget* const widget = views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<ToolbarActionsBarBubbleViews>(
          anchor_view ? anchor_view : extensions_button_,
          anchor_view != nullptr, std::move(controller)));

  ShowWidgetForExtension(widget, extension_id);
}

void ExtensionsToolbarContainer::ShowToolbarActionBubbleAsync(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) {
  ShowToolbarActionBubble(std::move(bubble));
}

void ExtensionsToolbarContainer::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->empty() || !selection.active_tab_changed())
    return;

  extensions::MaybeShowExtensionControlledNewTabPage(browser_,
                                                     selection.new_contents);
}

void ExtensionsToolbarContainer::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  CreateActionForId(action_id);
  ReorderViews();
  UpdateContainerVisibility();
}

void ExtensionsToolbarContainer::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  // TODO(pbos): Handle extension upgrades, see ToolbarActionsBar. Arguably this
  // could be handled inside the model and be invisible to the container when
  // permissions are unchanged.

  auto iter = std::find_if(
      actions_.begin(), actions_.end(),
      [action_id](const auto& item) { return item->GetId() == action_id; });
  DCHECK(iter != actions_.end());
  // Ensure the action outlives the UI element to perform any cleanup.
  std::unique_ptr<ToolbarActionViewController> controller = std::move(*iter);
  actions_.erase(iter);
  // Undo the popout, if necessary. Actions expect to not be popped out while
  // destroying.
  if (popped_out_action_ == controller.get())
    UndoPopOut();

  RemoveChildViewT(GetViewForId(action_id));
  icons_.erase(action_id);

  UpdateContainerVisibility();
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
    bool is_highlighting) {
  NOTREACHED()
      << "Action highlighting is not supported with the extensions menu";
}

void ExtensionsToolbarContainer::OnToolbarModelInitialized() {
  CreateActions();
}

void ExtensionsToolbarContainer::OnToolbarPinnedActionsChanged() {
  for (const auto& it : icons_)
    UpdateIconVisibility(it.first);
  ReorderViews();
}

void ExtensionsToolbarContainer::ReorderViews() {
  const auto& pinned_action_ids = model_->pinned_action_ids();
  for (size_t i = 0; i < pinned_action_ids.size(); ++i)
    ReorderChildView(GetViewForId(pinned_action_ids[i]), i);

  if (drop_info_.get())
    ReorderChildView(GetViewForId(drop_info_->action_id), drop_info_->index);

  // The extension button is always last.
  ReorderChildView(extensions_button_, -1);
}

void ExtensionsToolbarContainer::CreateActions() {
  DCHECK(icons_.empty());
  DCHECK(actions_.empty());

  // If the model isn't initialized, wait for it.
  if (!model_->actions_initialized())
    return;

  for (const auto& action_id : model_->action_ids())
    CreateActionForId(action_id);

  ReorderViews();
  UpdateContainerVisibility();
}

void ExtensionsToolbarContainer::CreateActionForId(
    const ToolbarActionsModel::ActionId& action_id) {
  actions_.push_back(
      model_->CreateActionForId(browser_, this, false, action_id));
  auto icon = std::make_unique<ToolbarActionView>(actions_.back().get(), this);
  // Set visibility before adding to prevent extraneous animation.
  icon->SetVisible(CanShowIconInToolbar() && model_->IsActionPinned(action_id));
  ObserveButton(icon.get());
  icons_.insert({action_id, AddChildView(std::move(icon))});
}

content::WebContents* ExtensionsToolbarContainer::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

bool ExtensionsToolbarContainer::ShownInsideMenu() const {
  return false;
}

bool ExtensionsToolbarContainer::CanShowIconInToolbar() const {
  // Pinning extensions is not available in PWAs.
  return !browser_->app_controller();
}

void ExtensionsToolbarContainer::OnToolbarActionViewDragDone() {}

views::LabelButton* ExtensionsToolbarContainer::GetOverflowReferenceView()
    const {
  return extensions_button_;
}

gfx::Size ExtensionsToolbarContainer::GetToolbarActionSize() {
  constexpr gfx::Size kDefaultSize(28, 28);
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  return browser_view
             ? browser_view->toolbar_button_provider()->GetToolbarButtonSize()
             : kDefaultSize;
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
  if (!CanShowIconInToolbar())
    return false;

  // Only pinned extensions should be draggable.
  auto it = std::find_if(model_->pinned_action_ids().cbegin(),
                         model_->pinned_action_ids().cend(),
                         [this, sender](const std::string& action_id) {
                           return GetViewForId(action_id) == sender;
                         });
  return it != model_->pinned_action_ids().cend();
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
  animating_layout_manager()->PostOrQueueAction(base::BindOnce(
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

const char* ExtensionsToolbarContainer::GetClassName() const {
  return "ExtensionsToolbarContainer";
}

void ExtensionsToolbarContainer::OnWidgetClosing(views::Widget* widget) {
  auto iter = std::find_if(
      anchored_widgets_.begin(), anchored_widgets_.end(),
      [widget](const auto& info) { return info.widget == widget; });
  DCHECK(iter != anchored_widgets_.end());
  iter->widget->RemoveObserver(this);
  const std::string extension_id = std::move(iter->extension_id);
  anchored_widgets_.erase(iter);
  UpdateIconVisibility(extension_id);
}

void ExtensionsToolbarContainer::OnWidgetDestroying(views::Widget* widget) {
  OnWidgetClosing(widget);
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
  extension_view->SetImageModel(
      views::Button::STATE_NORMAL,
      visible ? ui::ImageModel::FromImageSkia(GetExtensionIcon(extension_view))
              : ui::ImageModel());
}

void ExtensionsToolbarContainer::UpdateContainerVisibility() {
  // The container (and extensions-menu button) should be visible if we have at
  // least one extension.
  SetVisible(!actions_.empty());
}
