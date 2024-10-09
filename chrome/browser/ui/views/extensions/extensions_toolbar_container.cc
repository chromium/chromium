// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/toolbar_action_hover_card_types.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container_view_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {

using ::ui::mojom::DragOperation;

base::OnceClosure& GetOnVisibleCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

}  // namespace

void ExtensionsToolbarContainer::SetOnVisibleCallbackForTesting(
    base::OnceClosure callback) {
  GetOnVisibleCallbackForTesting() = std::move(callback);
}

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
      extensions_menu_coordinator_(
          base::FeatureList::IsEnabled(
              extensions_features::kExtensionsMenuAccessControl)
              ? std::make_unique<ExtensionsMenuCoordinator>(browser_)
              : nullptr),
      extensions_button_(
          new ExtensionsToolbarButton(browser,
                                      this,
                                      extensions_menu_coordinator_.get())),
      display_mode_(display_mode),
      action_hover_card_controller_(
          std::make_unique<ToolbarActionHoverCardController>(this)) {
  SetProperty(views::kElementIdentifierKey,
              kToolbarExtensionsContainerElementId);

  // The container shouldn't show unless / until we have extensions available.
  SetVisible(false);

  // So we only get enter/exit messages when the mouse enters/exits the whole
  // container, even if it is entering/exiting a specific toolbar action view,
  // too.
  SetNotifyEnterExitOnChild(true);

  // Add extensions button.
  AddMainItem(extensions_button_);

  // Create request access button.
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    auto request_access_button =
        std::make_unique<ExtensionsRequestAccessButton>(browser_, this);
    request_access_button->SetVisible(false);
    request_access_button_ = AddChildView(std::move(request_access_button));
  }

  // Create close side panel button.
  std::unique_ptr<ToolbarButton> close_side_panel_button =
      std::make_unique<ToolbarButton>(base::BindRepeating(
          &ExtensionsToolbarContainer::CloseSidePanelButtonPressed,
          base::Unretained(this)));
  close_side_panel_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SUBMENU_CLOSE_SIDE_PANEL_ITEM));
  close_side_panel_button->SetVisible(false);
  close_side_panel_button->SetProperty(views::kFlexBehaviorKey,
                                       views::FlexSpecification());
  close_side_panel_button_ = AddChildView(std::move(close_side_panel_button));
  UpdateCloseSidePanelButtonIcon();
  pref_change_registrar_.Init(browser_->profile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSidePanelHorizontalAlignment,
      base::BindRepeating(
          &ExtensionsToolbarContainer::UpdateCloseSidePanelButtonIcon,
          base::Unretained(this)));

  // Layout.
  const views::FlexSpecification hide_icon_flex_specification =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithWeight(0);
  GetTargetLayoutManager()
      ->SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse)
      .SetDefault(views::kFlexBehaviorKey,
                  hide_icon_flex_specification.WithOrder(3));

  switch (display_mode) {
    case DisplayMode::kNormal:
      // In normal mode, the buttons are always shown.
      extensions_button_->SetProperty(views::kFlexBehaviorKey,
                                      views::FlexSpecification());
      if (request_access_button_) {
        request_access_button_->SetProperty(views::kFlexBehaviorKey,
                                            views::FlexSpecification());
      }
      break;
    case DisplayMode::kCompact:
    case DisplayMode::kAutoHide:
      // In compact/auto hide mode, the buttons can be hidden according to flex
      // order preference.
      extensions_button_->SetProperty(
          views::kFlexBehaviorKey,
          hide_icon_flex_specification.WithOrder(
              ExtensionsToolbarContainerViewController::
                  kFlexOrderExtensionsButton));
      if (request_access_button_) {
        request_access_button_->SetProperty(
            views::kFlexBehaviorKey,
            hide_icon_flex_specification.WithOrder(
                ExtensionsToolbarContainerViewController::
                    kFlexOrderRequestAccessButton));
      }
      break;
  }

  GetTargetLayoutManager()->SetDefault(views::kMarginsKey,
                                       gfx::Insets::VH(0, 2));

  UpdateControlsVisibility();

  CreateActions();
}

ExtensionsToolbarContainer::~ExtensionsToolbarContainer() {
  // Eliminate the hover card first to avoid order-of-operation issues (e.g.
  // avoid events during teardown).
  action_hover_card_controller_.reset();

  close_side_panel_button_ = nullptr;

  // The child views hold pointers to the |actions_|, and thus need to be
  // destroyed before them.
  RemoveAllChildViews();

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

void ExtensionsToolbarContainer::CreateActions() {
  DCHECK(icons_.empty());
  DCHECK(actions_.empty());

  // If the model isn't initialized, wait for it.
  if (!model_->actions_initialized()) {
    return;
  }

  for (const auto& action_id : model_->action_ids()) {
    CreateActionForId(action_id);
  }

  ReorderAllChildViews();
  UpdateContainerVisibility();
}

void ExtensionsToolbarContainer::AddAction(
    const ToolbarActionsModel::ActionId& action_id) {
  CreateActionForId(action_id);
  ReorderAllChildViews();

  // Auto hide mode should not become visible due to extensions being added,
  // only due to user interaction.
  if (display_mode_ != DisplayMode::kAutoHide) {
    UpdateContainerVisibility();
  }

  UpdateControlsVisibility();

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExtensionsToolbarContainer::RemoveAction(
    const ToolbarActionsModel::ActionId& action_id) {
  // TODO(pbos): Handle extension upgrades, see ToolbarActionsBar. Arguably this
  // could be handled inside the model and be invisible to the container when
  // permissions are unchanged.

  auto iter = base::ranges::find(actions_, action_id,
                                 &ToolbarActionViewController::GetId);
  CHECK(iter != actions_.end(), base::NotFatalUntil::M130);
  // Ensure the action outlives the UI element to perform any cleanup.
  std::unique_ptr<ToolbarActionViewController> controller = std::move(*iter);
  actions_.erase(iter);

  // Undo the popout, if necessary. Actions expect to not be popped out while
  // destroying.
  if (popped_out_action_ == action_id) {
    UndoPopOut();
  }

  RemoveChildViewT(GetViewForId(action_id));
  icons_.erase(action_id);

  UpdateContainerVisibilityAfterAnimation();
  UpdateControlsVisibility();

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExtensionsToolbarContainer::UpdateAction(
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewController* action = GetActionForId(action_id);
  if (action) {
    action->UpdateState();
    ToolbarActionView* action_view = GetViewForId(action_id);
    // Only update hover card if it's currently showing for action, otherwise it
    // would mistakenly show the hover card.
    if (action_hover_card_controller_->IsHoverCardShowingForAction(
            action_view)) {
      action_hover_card_controller_->UpdateHoverCard(
          action_view, ToolbarActionHoverCardUpdateType::kToolbarActionUpdated);
    }
  }

  UpdateControlsVisibility();
}

void ExtensionsToolbarContainer::UpdatePinnedActions() {
  for (const auto& it : icons_) {
    UpdateIconVisibility(it.first);
  }
  ReorderAllChildViews();

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExtensionsToolbarContainer::UpdateExtensionsButton(
    extensions::PermissionsManager::UserSiteSetting site_setting,
    content::WebContents* web_contents,
    bool is_restricted_url) {
  // Extensions button state can only change when feature is enabled.
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    return;
  }

  ExtensionsToolbarButton::State extensions_button_state =
      ExtensionsToolbarButton::State::kDefault;

  if (is_restricted_url || site_setting ==
                               extensions::PermissionsManager::UserSiteSetting::
                                   kBlockAllExtensions) {
    extensions_button_state =
        ExtensionsToolbarButton::State::kAllExtensionsBlocked;
  } else if (ExtensionActionViewController::AnyActionHasCurrentSiteAccess(
                 actions_, web_contents)) {
    extensions_button_state =
        ExtensionsToolbarButton::State::kAnyExtensionHasAccess;
  }

  extensions_button_->UpdateState(extensions_button_state);
}

void ExtensionsToolbarContainer::UpdateRequestAccessButton(
    extensions::PermissionsManager::UserSiteSetting site_setting,
    content::WebContents* web_contents) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));

  // Button is never visible when actions cannot be show in toolbar.
  if (!model_->CanShowActionsInToolbar(*browser_)) {
    CHECK(!request_access_button_->GetVisible());
    return;
  }

  // Don't update the button if the confirmation message is currently showing;
  // it'll go away after a few seconds. Once the confirmation is collapsed,
  // button should be updated again.
  if (request_access_button_->IsShowingConfirmation()) {
    return;
  }

  // Extensions are included in the request access button only when:
  //   - site allows customizing site access by extension
  //   - extension added a request that has not been dismised
  //   - requests can be shown in the toolbar
  std::vector<extensions::ExtensionId> extensions;
  if (site_setting ==
      extensions::PermissionsManager::UserSiteSetting::kCustomizeByExtension) {
    int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
    auto* permissions_manager =
        extensions::PermissionsManager::Get(browser_->profile());
    auto site_permissions_helper =
        extensions::SitePermissionsHelper(browser_->profile());

    for (const auto& action : actions_) {
      std::string action_id = action->GetId();
      bool has_active_request =
          permissions_manager->HasActiveSiteAccessRequest(tab_id, action_id);
      bool can_show_access_requests_in_toolbar =
          site_permissions_helper.ShowAccessRequestsInToolbar(action_id);

      if (has_active_request && can_show_access_requests_in_toolbar) {
        extensions.push_back(action->GetId());
      }
    }
  }

  request_access_button_->Update(extensions);

  // Extensions button has left flat edge iff request access button is visible.
  // This will also update the button's background.
  std::optional<ToolbarButton::Edge> extensions_button_edge =
      request_access_button_->GetVisible()
          ? std::optional<ToolbarButton::Edge>(ToolbarButton::Edge::kLeft)
          : std::nullopt;
  extensions_button_->SetFlatEdge(extensions_button_edge);
}

void ExtensionsToolbarContainer::UpdateAllIcons() {
  UpdateControlsVisibility();

  for (const auto& action : actions_)
    action->UpdateState();

  if (close_side_panel_button_) {
    close_side_panel_button_->UpdateIcon();
  }
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
  GetAnimatingLayoutManager()->PostOrQueueAction(base::BindOnce(
      &ExtensionsToolbarContainer::AnchorAndShowWidgetImmediately,
      weak_ptr_factory_.GetWeakPtr(),
      // This is safe as `widget` is checked for membership in
      // `anchored_widgets_` which has ownership.
      base::UnsafeDangling(widget)));
}

views::Widget*
ExtensionsToolbarContainer::GetAnchoredWidgetForExtensionForTesting(
    const std::string& extension_id) {
  auto iter = base::ranges::find(anchored_widgets_, extension_id,
                                 &AnchoredWidget::extension_id);
  return iter == anchored_widgets_.end() ? nullptr : iter->widget.get();
}

bool ExtensionsToolbarContainer::IsExtensionsMenuShowing() const {
  return base::FeatureList::IsEnabled(
             extensions_features::kExtensionsMenuAccessControl)
             ? extensions_menu_coordinator_->IsShowing()
             : ExtensionsMenuView::IsShowing();
}

void ExtensionsToolbarContainer::HideExtensionsMenu() {
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl))
    extensions_menu_coordinator_->Hide();
  else
    ExtensionsMenuView::Hide();
}

bool ExtensionsToolbarContainer::ShouldForceVisibility(
    const std::string& extension_id) const {
  if (popped_out_action_.has_value() &&
      popped_out_action_.value() == extension_id) {
    return true;
  }

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
      case DisplayMode::kAutoHide:
        views::MinimumFlexSizeRule min_flex_rule =
            views::MinimumFlexSizeRule::kPreferredSnapToZero;
        BrowserView* const browser_view =
            BrowserView::GetBrowserViewForBrowser(browser_);
        if (browser_view->IsWindowControlsOverlayEnabled())
          min_flex_rule = views::MinimumFlexSizeRule::kPreferred;

        // In compact/auto hide mode, the icon can still drop out, but receives
        // precedence over other actions.
        action_view->SetProperty(
            views::kFlexBehaviorKey,
            views::FlexSpecification(min_flex_rule,
                                     views::MaximumFlexSizeRule::kPreferred)
                .WithWeight(0)
                .WithOrder(ExtensionsToolbarContainerViewController::
                               kFlexOrderActionView));
        break;
    }
  } else {
    action_view->ClearProperty(views::kFlexBehaviorKey);
  }

  if (must_show || (ToolbarActionsModel::CanShowActionsInToolbar(*browser_) &&
                    model_->IsActionPinned(extension_id))) {
    GetAnimatingLayoutManager()->FadeIn(action_view);
  } else {
    GetAnimatingLayoutManager()->FadeOut(action_view);
  }
}

void ExtensionsToolbarContainer::AnchorAndShowWidgetImmediately(
    MayBeDangling<views::Widget> widget) {
  auto iter =
      base::ranges::find(anchored_widgets_, widget, &AnchoredWidget::widget);

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
                                               : GetExtensionsButton());
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

std::optional<extensions::ExtensionId>
ExtensionsToolbarContainer::GetPoppedOutActionId() const {
  return popped_out_action_;
}

void ExtensionsToolbarContainer::OnContextMenuShownFromToolbar(
    const std::string& action_id) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40124221): Remove hiding active popup here once this bug is
  // fixed.
  HideActivePopup();
#endif

    extension_with_open_context_menu_id_ = action_id;
    UpdateIconVisibility(extension_with_open_context_menu_id_.value());
}

void ExtensionsToolbarContainer::OnContextMenuClosedFromToolbar() {
  CHECK(extension_with_open_context_menu_id_.has_value());

  extensions::ExtensionId const extension_id =
      extension_with_open_context_menu_id_.value();
  extension_with_open_context_menu_id_.reset();
  UpdateIconVisibility(extension_id);
}

bool ExtensionsToolbarContainer::IsActionVisibleOnToolbar(
    const std::string& action_id) const {
  return model_->IsActionPinned(action_id) || ShouldForceVisibility(action_id);
}

void ExtensionsToolbarContainer::UndoPopOut() {
  DCHECK(popped_out_action_);
  const extensions::ExtensionId popped_out_action = popped_out_action_.value();
  popped_out_action_ = std::nullopt;
  UpdateIconVisibility(popped_out_action);
  UpdateContainerVisibilityAfterAnimation();
}

void ExtensionsToolbarContainer::SetPopupOwner(
    ToolbarActionViewController* popup_owner) {
  // We should never be setting a popup owner when one already exists, and
  // never unsetting one when one wasn't set.
  DCHECK((popup_owner_ != nullptr) ^ (popup_owner != nullptr));
  popup_owner_ = popup_owner;

  // Container should become visible if |popup_owner_| and may lose visibility
  // if not |popup_owner_|. Visibility must be maintained during layout
  // animations.
  if (popup_owner_)
    UpdateContainerVisibility();
  else
    UpdateContainerVisibilityAfterAnimation();
}

void ExtensionsToolbarContainer::HideActivePopup() {
  if (popup_owner_)
    popup_owner_->HidePopup();
  DCHECK(!popup_owner_);
  UpdateContainerVisibilityAfterAnimation();
}

bool ExtensionsToolbarContainer::CloseOverflowMenuIfOpen() {
  if (IsExtensionsMenuShowing()) {
    HideExtensionsMenu();
    return true;
  }
  return false;
}

void ExtensionsToolbarContainer::PopOutAction(
    const extensions::ExtensionId& action_id,
    base::OnceClosure closure) {
  // TODO(pbos): Highlight popout differently.
  DCHECK(!popped_out_action_.has_value());
  popped_out_action_ = action_id;
  UpdateIconVisibility(action_id);
  GetAnimatingLayoutManager()->PostOrQueueAction(std::move(closure));
  UpdateContainerVisibility();
}

bool ExtensionsToolbarContainer::ShowToolbarActionPopupForAPICall(
    const std::string& action_id,
    ShowPopupCallback callback) {
  // Don't override another popup, and only show in the active window.
  if (popped_out_action_ || !browser_->window()->IsActive())
    return false;

  ToolbarActionViewController* action = GetActionForId(action_id);
  DCHECK(action);
  action->TriggerPopupForAPI(std::move(callback));

  return true;
}

void ExtensionsToolbarContainer::ShowToolbarActionBubble(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> controller) {
  const std::string extension_id = controller->GetAnchorActionId();

  views::View* const anchor_view = GetViewForId(extension_id);

  views::Widget* const widget = views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<ToolbarActionsBarBubbleViews>(
          anchor_view ? anchor_view : GetExtensionsButton(),
          anchor_view != nullptr, std::move(controller)));

  ShowWidgetForExtension(widget, extension_id);
}

void ExtensionsToolbarContainer::ToggleExtensionsMenu() {
  GetExtensionsButton()->ToggleExtensionsMenu();
}

bool ExtensionsToolbarContainer::HasAnyExtensions() const {
  return !actions_.empty();
}

void ExtensionsToolbarContainer::ReorderAllChildViews() {
  // Reorder pinned action views left-to-right.
  const auto& pinned_action_ids = model_->pinned_action_ids();
  for (size_t i = 0; i < pinned_action_ids.size(); ++i) {
    ReorderChildView(GetViewForId(pinned_action_ids[i]), i);
  }
  if (drop_info_.get()) {
    ReorderChildView(GetViewForId(drop_info_->action_id), drop_info_->index);
  }

  // Reorder other buttons right-to-left. This guarantees popped out action
  // views will appear in between pinned action views and other buttons. We
  // don't reorder popped out action views because they should appear in the
  // order they were triggered.
  int button_index = children().size() - 1;

  if (close_side_panel_button_) {
    // The close side panel button is always last.
    ReorderChildView(close_side_panel_button_, button_index--);
  }

  // The extension button is always second to last if `close_side_panel_button_`
  // exists, or last otherwise.
  ReorderChildView(main_item(), button_index--);

  if (request_access_button_) {
    // The request access button is always third to last if
    // `close_side_panel_button_` exists, or second to last otherwise.
    ReorderChildView(request_access_button_, button_index);
  }
}

void ExtensionsToolbarContainer::CreateActionForId(
    const ToolbarActionsModel::ActionId& action_id) {
  actions_.push_back(
      ExtensionActionViewController::Create(action_id, browser_, this));
  auto icon = std::make_unique<ToolbarActionView>(actions_.back().get(), this);
  // Set visibility before adding to prevent extraneous animation.
  icon->SetVisible(ToolbarActionsModel::CanShowActionsInToolbar(*browser_) &&
                   model_->IsActionPinned(action_id));
  views::FocusRing::Get(icon.get())->SetOutsetFocusRingDisabled(true);
  ObserveButton(icon.get());
  icons_.insert({action_id, AddChildView(std::move(icon))});
}

content::WebContents* ExtensionsToolbarContainer::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

views::LabelButton* ExtensionsToolbarContainer::GetOverflowReferenceView()
    const {
  return GetExtensionsButton();
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

  auto it = base::ranges::find(
      model_->pinned_action_ids(), sender,
      [this](const std::string& action_id) { return GetViewForId(action_id); });
  DCHECK(it != model_->pinned_action_ids().cend());
  ToolbarActionView* extension_view = GetViewForId(*it);

  ui::ImageModel icon = GetExtensionIcon(extension_view);
  data->provider().SetDragImage(icon.Rasterize(GetColorProvider()),
                                press_pt.OffsetFromOrigin());

  // Fill in the remaining info.
  size_t index = it - model_->pinned_action_ids().cbegin();
  BrowserActionDragData drag_data(extension_view->view_controller()->GetId(),
                                  index);
  drag_data.Write(browser_->profile(), data);
}

int ExtensionsToolbarContainer::GetDragOperationsForView(View* sender,
                                                         const gfx::Point& p) {
  return browser_->profile()->IsOffTheRecord() ? ui::DragDropTypes::DRAG_NONE
                                               : ui::DragDropTypes::DRAG_MOVE;
}

bool ExtensionsToolbarContainer::CanStartDragForView(View* sender,
                                                     const gfx::Point& press_pt,
                                                     const gfx::Point& p) {
  // We don't allow dragging if the container isn't in the toolbar, or if
  // the profile is incognito (to avoid changing state from an incognito
  // window).
  if (!ToolbarActionsModel::CanShowActionsInToolbar(*browser_) ||
      browser_->profile()->IsOffTheRecord()) {
    return false;
  }

  // Only pinned extensions should be draggable.
  auto it = base::ranges::find(
      model_->pinned_action_ids(), sender,
      [this](const std::string& action_id) { return GetViewForId(action_id); });
  if (it == model_->pinned_action_ids().cend())
    return false;

  // TODO(crbug.com/40808374): Force-pinned extensions are not draggable.
  return !model_->IsActionForcePinned(*it);
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

void ExtensionsToolbarContainer::OnDragEntered(
    const ui::DropTargetEvent& event) {
  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

int ExtensionsToolbarContainer::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data()))
    return ui::DragDropTypes::DRAG_NONE;

  // Check if there is an extension for the dragged icon (e.g. an extension can
  // be de deleted while dragging its icon).
  if (!GetActionForId(data.id()))
    return ui::DragDropTypes::DRAG_NONE;

  size_t before_icon = 0;
  // Figure out where to display the icon during dragging transition.

  // First, since we want to update the dragged extension's position from before
  // an icon to after it when the event passes the midpoint between two icons.
  // This will convert the event coordinate into the index of the icon we want
  // to display the dragged extension before. We also mirror the event.x() so
  // that our calculations are consistent with left-to-right.
  const int offset_into_icon_area = GetMirroredXInView(event.x());
  const size_t before_icon_unclamped = WidthToIconCount(offset_into_icon_area);

  const size_t visible_icons = model_->pinned_action_ids().size();

  // Because the user can drag outside the container bounds, we need to clamp
  // to the valid range. Note that the maximum allowable value is
  // |visible_icons|, not (|visible_icons| - 1), because we represent the
  // dragged extension being past the last icon as being "before the (last + 1)
  // icon".
  before_icon = std::min(before_icon_unclamped, visible_icons);

  if (!drop_info_.get() || drop_info_->index != before_icon) {
    drop_info_ = std::make_unique<DropInfo>(data.id(), before_icon);
    SetExtensionIconVisibility(drop_info_->action_id, false);
    ReorderAllChildViews();
  }

  return ui::DragDropTypes::DRAG_MOVE;
}

void ExtensionsToolbarContainer::OnDragExited() {
  if (!drop_info_)
    return;

  const ToolbarActionsModel::ActionId dragged_extension_id =
      drop_info_->action_id;
  drop_info_.reset();
  DragDropCleanup(dragged_extension_id);
}

views::View::DropCallback ExtensionsToolbarContainer::GetDropCallback(
    const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data()))
    return base::NullCallback();

  auto action_id = std::move(drop_info_->action_id);
  auto index = drop_info_->index;
  drop_info_.reset();
  base::ScopedClosureRunner cleanup(
      base::BindOnce(&ExtensionsToolbarContainer::DragDropCleanup,
                     weak_ptr_factory_.GetWeakPtr(), action_id));
  return base::BindOnce(&ExtensionsToolbarContainer::MovePinnedAction,
                        drop_weak_ptr_factory_.GetWeakPtr(), action_id, index,
                        std::move(cleanup));
}

void ExtensionsToolbarContainer::OnWidgetDestroying(views::Widget* widget) {
  auto iter =
      base::ranges::find(anchored_widgets_, widget, &AnchoredWidget::widget);
  CHECK(iter != anchored_widgets_.end(), base::NotFatalUntil::M130);
  iter->widget->RemoveObserver(this);
  const std::string extension_id = std::move(iter->extension_id);
  anchored_widgets_.erase(iter);
  UpdateIconVisibility(extension_id);
}

size_t ExtensionsToolbarContainer::WidthToIconCount(int x_offset) {
  const int element_padding = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  size_t unclamped_count =
      std::max((x_offset + element_padding) /
                   (GetToolbarActionSize().width() + element_padding),
               0);
  return std::min(unclamped_count, actions_.size());
}

ui::ImageModel ExtensionsToolbarContainer::GetExtensionIcon(
    ToolbarActionView* extension_view) {
  return extension_view->view_controller()->GetIcon(GetCurrentWebContents(),
                                                    GetToolbarActionSize());
}

void ExtensionsToolbarContainer::SetExtensionIconVisibility(
    ToolbarActionsModel::ActionId id,
    bool visible) {
  auto it = base::ranges::find(
      model_->pinned_action_ids(), GetViewForId(id),
      [this](const std::string& action_id) { return GetViewForId(action_id); });
  if (it == model_->pinned_action_ids().cend())
    return;

  ToolbarActionView* extension_view = GetViewForId(*it);
  if (!extension_view)
    return;

  extension_view->SetImageModel(
      views::Button::STATE_NORMAL,
      visible ? GetExtensionIcon(extension_view) : ui::ImageModel());
}

void ExtensionsToolbarContainer::UpdateContainerVisibility() {
  bool was_visible = GetVisible();
  SetVisible(ShouldContainerBeVisible());

  // Layout animation does not handle host view visibility changing; requires
  // resetting.
  if (was_visible != GetVisible())
    GetAnimatingLayoutManager()->ResetLayout();

  if (!was_visible && GetVisible() && GetOnVisibleCallbackForTesting())
    std::move(GetOnVisibleCallbackForTesting()).Run();
}

bool ExtensionsToolbarContainer::ShouldContainerBeVisible() const {
  // The container (and extensions-menu button) should not be visible if we have
  // no extensions.
  if (!HasAnyExtensions())
    return false;

  // All other display modes are constantly visible.
  if (display_mode_ != DisplayMode::kAutoHide)
    return true;

  if (GetAnimatingLayoutManager()->is_animating())
    return true;

  // Is menu showing.
  if (GetExtensionsButton()->GetExtensionsMenuShowing())
    return true;

  // Is extension pop out is showing.
  if (popped_out_action_)
    return true;

  // Is extension pop up showing.
  if (popup_owner_)
    return true;

  return false;
}

void ExtensionsToolbarContainer::UpdateContainerVisibilityAfterAnimation() {
  GetAnimatingLayoutManager()->PostOrQueueAction(
      base::BindOnce(&ExtensionsToolbarContainer::UpdateContainerVisibility,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionsToolbarContainer::OnMenuOpening() {
  // Record IPH usage, which should only be shown when any extension has access.
  if (GetExtensionsButton()->state() ==
      ExtensionsToolbarButton::State::kAnyExtensionHasAccess) {
    browser_->window()->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHExtensionsMenuFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  } else {
    // Otherwise, just close the IPH if it's present.
    browser_->window()->AbortFeaturePromo(
        feature_engagement::kIPHExtensionsMenuFeature);
  }

  UpdateContainerVisibility();
}

void ExtensionsToolbarContainer::OnMenuClosed() {
  UpdateContainerVisibility();
}

void ExtensionsToolbarContainer::UpdateSidePanelState(bool is_active) {
  close_side_panel_button_->SetVisible(is_active);
  if (is_active) {
    close_side_panel_button_anchor_highlight_ =
        close_side_panel_button_->AddAnchorHighlight();
  } else {
    close_side_panel_button_anchor_highlight_.reset();
  }
}

void ExtensionsToolbarContainer::MovePinnedAction(
    const ToolbarActionsModel::ActionId& action_id,
    size_t index,
    base::ScopedClosureRunner cleanup,
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  model_->MovePinnedAction(action_id, index);

  output_drag_op = DragOperation::kMove;
  // `cleanup` will run automatically when it goes out of scope to finish
  // up the drag.
}

void ExtensionsToolbarContainer::DragDropCleanup(
    const ToolbarActionsModel::ActionId& dragged_extension_id) {
  ReorderAllChildViews();
  GetAnimatingLayoutManager()->PostOrQueueAction(base::BindOnce(
      &ExtensionsToolbarContainer::SetExtensionIconVisibility,
      weak_ptr_factory_.GetWeakPtr(), dragged_extension_id, true));
}

void ExtensionsToolbarContainer::UpdateControlsVisibility() {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    return;
  }

  content::WebContents* web_contents = GetCurrentWebContents();
  if (!web_contents)
    return;

  bool is_restricted_url =
      model_->IsRestrictedUrl(web_contents->GetLastCommittedURL());
  extensions::PermissionsManager::UserSiteSetting site_setting =
      extensions::PermissionsManager::Get(browser_->profile())
          ->GetUserSiteSetting(
              web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  UpdateExtensionsButton(site_setting, web_contents, is_restricted_url);
  UpdateRequestAccessButton(site_setting, web_contents);
}

void ExtensionsToolbarContainer::CloseSidePanelButtonPressed() {
  browser_->GetFeatures().side_panel_ui()->Close();
}

void ExtensionsToolbarContainer::UpdateToolbarActionHoverCard(
    ToolbarActionView* action_view,
    ToolbarActionHoverCardUpdateType update_type) {
  action_hover_card_controller_->UpdateHoverCard(action_view, update_type);
}

void ExtensionsToolbarContainer::CollapseConfirmation() {
  if (!request_access_button_->IsShowingConfirmation()) {
    return;
  }

  request_access_button_->ResetConfirmation();
  UpdateControlsVisibility();
}

void ExtensionsToolbarContainer::OnMouseExited(const ui::MouseEvent& event) {
  UpdateToolbarActionHoverCard(nullptr,
                               ToolbarActionHoverCardUpdateType::kHover);
}

void ExtensionsToolbarContainer::OnMouseMoved(const ui::MouseEvent& event) {
  // Since we set the container's "notify enter exit on child" to true, we can
  // get notified when the mouse enters a child view only if it originates from
  // outside the container. This means that we a) can know when the mouse enters
  // a toolbar action view (which is handled in such class) and b) cannot
  // know when the mouse leaves a toolbar action view and enters a toolbar
  // control. Therefore, listening for on mouse moved in the container reflects
  // moving the mouse from toolbar action view to toolbar controls.
  UpdateToolbarActionHoverCard(nullptr,
                               ToolbarActionHoverCardUpdateType::kHover);
}

void ExtensionsToolbarContainer::UpdateCloseSidePanelButtonIcon() {
  const bool is_right_aligned = browser_->profile()->GetPrefs()->GetBoolean(
      prefs::kSidePanelHorizontalAlignment);
  close_side_panel_button_->SetVectorIcon(
      is_right_aligned ? kRightPanelCloseIcon : kLeftPanelCloseIcon);
}

BEGIN_METADATA(ExtensionsToolbarContainer)
END_METADATA
