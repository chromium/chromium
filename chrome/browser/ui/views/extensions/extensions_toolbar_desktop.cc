// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/toolbar/toolbar_action_hover_card_types.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/extensions/extension_action_delegate_desktop.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop_view_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "ui/base/dragdrop/drag_drop_types.h"
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

// Only attempt to launch the Extensions Zero State Promo IPH after this
// timestamp, in order to reduce the cadence with which the this IPH calls
// the User Educations system.
std::optional<base::TimeTicks> g_zero_state_promo_next_show_time_opt =
    std::nullopt;

// The interval of time to wait for between attempting to launch the Zero
// State Promo.
constexpr base::TimeDelta kZeroStatePromoIntervalBetweenLaunchAttempt =
    base::Minutes(2);

bool ArePromotionsEnabled() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state && local_state->GetBoolean(prefs::kPromotionsEnabled);
}

}  // namespace

void ExtensionsToolbarDesktop::SetOnVisibleCallbackForTesting(
    base::OnceClosure callback) {
  GetOnVisibleCallbackForTesting() = std::move(callback);
}

// static
void ExtensionsToolbarDesktop::WakeZeroStatePromoForTesting() {
  g_zero_state_promo_next_show_time_opt = base::TimeTicks::Now();
}

struct ExtensionsToolbarDesktop::DropInfo {
  DropInfo(ToolbarActionsModel::ActionId action_id, size_t index);

  // The id for the action being dragged.
  ToolbarActionsModel::ActionId action_id;

  // The (0-indexed) icon before the action will be dropped.
  size_t index;
};

ExtensionsToolbarDesktop::DropInfo::DropInfo(
    ToolbarActionsModel::ActionId action_id,
    size_t index)
    : action_id(action_id), index(index) {}

ExtensionsToolbarDesktop::ExtensionsToolbarDesktop(Browser* browser,
                                                   DisplayMode display_mode)
    : ToolbarIconContainerView(/*uses_highlight=*/true),
      browser_(browser),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      display_mode_(display_mode),
      action_hover_card_controller_(
          std::make_unique<ToolbarActionHoverCardController>(this)),
      toolbar_view_model_(
          std::make_unique<ExtensionsToolbarViewModel>(this, browser, model_)),
      scoped_toolbar_view_model_user_data_(browser->GetUnownedUserDataHost(),
                                           *toolbar_view_model_),
      extensions_menu_coordinator_(
          base::FeatureList::IsEnabled(
              extensions_features::kExtensionsMenuAccessControl)
              ? std::make_unique<ExtensionsMenuCoordinator>(
                    browser,
                    toolbar_view_model_.get())
              : nullptr),
      extensions_button_(
          new ExtensionsToolbarButton(browser,
                                      this,
                                      extensions_menu_coordinator_.get())) {
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
        std::make_unique<ExtensionsRequestAccessButton>(
            browser_, toolbar_view_model_.get(), this);
    request_access_button->SetVisible(false);
    request_access_button_ = AddChildView(std::move(request_access_button));
  }

  // Create close side panel button.
  std::unique_ptr<ToolbarButton> close_side_panel_button =
      std::make_unique<ToolbarButton>(base::BindRepeating(
          &ExtensionsToolbarDesktop::CloseSidePanelButtonPressed,
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
          &ExtensionsToolbarDesktop::UpdateCloseSidePanelButtonIcon,
          base::Unretained(this)));

  // Layout.
  const views::FlexSpecification hide_icon_flex_specification =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithWeight(0);
  GetTargetLayoutManager()
      ->SetFlexAllocationOrder(views::FlexAllocationOrder::kNormal)
      .SetDefault(views::kFlexBehaviorKey,
                  hide_icon_flex_specification.WithOrder(
                      ExtensionsToolbarDesktopViewController::
                          kFlexOrderExtensionsButton));

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
          views::kFlexBehaviorKey, hide_icon_flex_specification.WithOrder(
                                       ExtensionsToolbarDesktopViewController::
                                           kFlexOrderExtensionsButton));
      if (request_access_button_) {
        request_access_button_->SetProperty(
            views::kFlexBehaviorKey,
            hide_icon_flex_specification.WithOrder(
                ExtensionsToolbarDesktopViewController::
                    kFlexOrderRequestAccessButton));
      }
      break;
  }

  GetTargetLayoutManager()->SetDefault(views::kMarginsKey,
                                       gfx::Insets::VH(0, 2));

  UpdateControlsVisibility();

  toolbar_view_model_observation_.Observe(toolbar_view_model_.get());

  if (toolbar_view_model_->AreActionsInitialized()) {
    // Since we added the observer in this constructor, we missed the observer
    // call.
    OnActionsInitialized();
  }
}

ExtensionsToolbarDesktop::~ExtensionsToolbarDesktop() {
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
  for (const auto& anchored_widget : anchored_widgets_) {
    widgets.push_back(anchored_widget.widget);
  }
  for (auto* widget : widgets) {
    widget->CloseNow();
  }
  // The widgets should close synchronously (resulting in OnWidgetClosing()),
  // so |anchored_widgets_| should now be empty.
  DCHECK(anchored_widgets_.empty());
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void ExtensionsToolbarDesktop::UpdateExtensionsButton(
    content::WebContents& web_contents) {
  // Extensions button state can only change when feature is enabled.
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    return;
  }

  ExtensionsToolbarViewModel::ExtensionsToolbarButtonState state =
      toolbar_view_model_->GetButtonState(web_contents);
  extensions_button_->UpdateState(state);
}

void ExtensionsToolbarDesktop::UpdateRequestAccessButton(
    content::WebContents& web_contents) {
  // Extensions request access button can only be updated when feature is
  // enabled.
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    return;
  }

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

  ExtensionsToolbarViewModel::RequestAccessButtonParams button_params =
      toolbar_view_model_->GetRequestAccessButtonParams(&web_contents);

  request_access_button_->Update(button_params);

  // Extensions button has left flat edge iff request access button is visible.
  // This will also update the button's background.
  std::optional<ToolbarButton::Edge> extensions_button_edge =
      request_access_button_->GetVisible()
          ? std::optional<ToolbarButton::Edge>(ToolbarButton::Edge::kLeft)
          : std::nullopt;
  extensions_button_->SetFlatEdge(extensions_button_edge);
}

void ExtensionsToolbarDesktop::UpdateAllIcons() {
  UpdateControlsVisibility();

  for (const auto& icon : icons_) {
    icon.second->UpdateState();
  }

  if (close_side_panel_button_) {
    close_side_panel_button_->UpdateIcon();
  }
}

ToolbarActionView* ExtensionsToolbarDesktop::GetViewForId(
    const std::string& id) {
  const auto it = icons_.find(id);
  return (it == icons_.end()) ? nullptr : it->second;
}

void ExtensionsToolbarDesktop::ShowWidgetForExtension(
    views::Widget* widget,
    const std::string& extension_id) {
  anchored_widgets_.push_back({widget, extension_id});
  widget->AddObserver(this);
  UpdateIconVisibility(extension_id);
  GetAnimatingLayoutManager()->PostOrQueueAction(
      base::BindOnce(&ExtensionsToolbarDesktop::AnchorAndShowWidgetImmediately,
                     weak_ptr_factory_.GetWeakPtr(),
                     // This is safe as `widget` is checked for membership in
                     // `anchored_widgets_` which has ownership.
                     base::UnsafeDangling(widget)));
}

views::Widget*
ExtensionsToolbarDesktop::GetAnchoredWidgetForExtensionForTesting(
    const std::string& extension_id) {
  auto iter = std::ranges::find(anchored_widgets_, extension_id,
                                &AnchoredWidget::extension_id);
  return iter == anchored_widgets_.end() ? nullptr : iter->widget.get();
}

bool ExtensionsToolbarDesktop::IsExtensionsMenuShowing() const {
  return base::FeatureList::IsEnabled(
             extensions_features::kExtensionsMenuAccessControl)
             ? extensions_menu_coordinator_->IsShowing()
             : ExtensionsMenuView::IsShowing();
}

void ExtensionsToolbarDesktop::HideExtensionsMenu() {
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    extensions_menu_coordinator_->Hide();
  } else {
    ExtensionsMenuView::Hide();
  }
}

bool ExtensionsToolbarDesktop::ShouldForceVisibility(
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
    if (anchored_widget.extension_id == extension_id) {
      return true;
    }
  }

  return false;
}

void ExtensionsToolbarDesktop::UpdateIconVisibility(
    const std::string& extension_id) {
  if (!GetWidget() || GetWidget()->IsClosed()) {
    return;
  }

  ToolbarActionView* const action_view = GetViewForId(extension_id);
  if (!action_view) {
    return;
  }

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
        if (browser_view->IsWindowControlsOverlayEnabled()) {
          min_flex_rule = views::MinimumFlexSizeRule::kPreferred;
        }

        // In compact/auto hide mode, the icon can still drop out, but receives
        // precedence over other actions.
        action_view->SetProperty(
            views::kFlexBehaviorKey,
            views::FlexSpecification(min_flex_rule,
                                     views::MaximumFlexSizeRule::kPreferred)
                .WithWeight(0)
                .WithOrder(ExtensionsToolbarDesktopViewController::
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

void ExtensionsToolbarDesktop::AnchorAndShowWidgetImmediately(
    MayBeDangling<views::Widget> widget) {
  auto iter =
      std::ranges::find(anchored_widgets_, widget, &AnchoredWidget::widget);

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

std::optional<extensions::ExtensionId>
ExtensionsToolbarDesktop::GetPoppedOutActionId() const {
  return popped_out_action_;
}

bool ExtensionsToolbarDesktop::IsActionVisibleOnToolbar(
    const std::string& action_id) const {
  return model_->IsActionPinned(action_id) || ShouldForceVisibility(action_id);
}

void ExtensionsToolbarDesktop::UndoPopOut() {
  DCHECK(popped_out_action_);
  const extensions::ExtensionId popped_out_action = popped_out_action_.value();
  popped_out_action_ = std::nullopt;
  UpdateIconVisibility(popped_out_action);
  UpdateContainerVisibilityAfterAnimation();
}

void ExtensionsToolbarDesktop::SetPopupOwner(
    ToolbarActionViewModel* popup_owner) {
  // We should never be setting a popup owner when one already exists, and
  // never unsetting one when one wasn't set.
  DCHECK((popup_owner_ != nullptr) ^ (popup_owner != nullptr));
  popup_owner_ = popup_owner;

  // Container should become visible if |popup_owner_| and may lose visibility
  // if not |popup_owner_|. Visibility must be maintained during layout
  // animations.
  if (popup_owner_) {
    UpdateContainerVisibility();
  } else {
    UpdateContainerVisibilityAfterAnimation();
  }
}

void ExtensionsToolbarDesktop::PopOutAction(
    const extensions::ExtensionId& action_id,
    base::OnceClosure closure) {
  // TODO(pbos): Highlight popout differently.
  DCHECK(!popped_out_action_.has_value());
  popped_out_action_ = action_id;
  UpdateIconVisibility(action_id);
  GetAnimatingLayoutManager()->PostOrQueueAction(std::move(closure));
  UpdateContainerVisibility();
}

void ExtensionsToolbarDesktop::ReorderAllChildViews() {
  // Reorder pinned action views left-to-right.
  const auto& pinned_action_ids = toolbar_view_model_->GetPinnedActionIds();
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

void ExtensionsToolbarDesktop::CreateActionViewForId(
    const ToolbarActionsModel::ActionId& action_id) {
  auto icon = std::make_unique<ToolbarActionView>(
      toolbar_view_model_->GetActionModelForId(action_id), this);
  CHECK(icon);

  // Set visibility before adding to prevent extraneous animation.
  icon->SetVisible(ToolbarActionsModel::CanShowActionsInToolbar(*browser_) &&
                   model_->IsActionPinned(action_id));
  views::FocusRing::Get(icon.get())->SetOutsetFocusRingDisabled(true);
  ObserveButton(icon.get());
  icons_.insert({action_id, AddChildView(std::move(icon))});
}

content::WebContents* ExtensionsToolbarDesktop::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

views::LabelButton* ExtensionsToolbarDesktop::GetOverflowReferenceView() const {
  return GetExtensionsButton();
}

gfx::Size ExtensionsToolbarDesktop::GetToolbarActionSize() {
  constexpr gfx::Size kDefaultSize(28, 28);
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  return browser_view
             ? browser_view->toolbar_button_provider()->GetToolbarButtonSize()
             : kDefaultSize;
}

void ExtensionsToolbarDesktop::MovePinnedActionBy(const std::string& action_id,
                                                  int move_by) {
  toolbar_view_model_->MovePinnedActionBy(action_id, move_by);
}

void ExtensionsToolbarDesktop::UpdateHoverCard(
    ToolbarActionView* action_view,
    ToolbarActionHoverCardUpdateType update_type) {
  action_hover_card_controller_->UpdateHoverCard(action_view, update_type);
}

void ExtensionsToolbarDesktop::OnContextMenuShown(
    const std::string& action_id) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40124221): Remove hiding active popup here once this bug is
  // fixed.
  HideActivePopup();
#endif

  extension_with_open_context_menu_id_ = action_id;
  UpdateIconVisibility(extension_with_open_context_menu_id_.value());
}

void ExtensionsToolbarDesktop::OnContextMenuClosed(
    const std::string& action_id) {
  CHECK(extension_with_open_context_menu_id_.has_value());

  extensions::ExtensionId const extension_id =
      extension_with_open_context_menu_id_.value();
  extension_with_open_context_menu_id_.reset();
  UpdateIconVisibility(extension_id);
}

void ExtensionsToolbarDesktop::WriteDragDataForView(View* sender,
                                                    const gfx::Point& press_pt,
                                                    ui::OSExchangeData* data) {
  DCHECK(data);

  auto it = std::ranges::find(
      toolbar_view_model_->GetPinnedActionIds(), sender,
      [this](const std::string& action_id) { return GetViewForId(action_id); });
  DCHECK(it != toolbar_view_model_->GetPinnedActionIds().cend());
  ToolbarActionView* extension_view = GetViewForId(*it);

  ui::ImageModel icon = GetExtensionIcon(extension_view);
  data->provider().SetDragImage(icon.Rasterize(GetColorProvider()),
                                press_pt.OffsetFromOrigin());

  // Fill in the remaining info.
  size_t index = it - toolbar_view_model_->GetPinnedActionIds().cbegin();
  BrowserActionDragData drag_data(extension_view->view_model()->GetId(), index);
  drag_data.Write(browser_->profile(), data);
}

int ExtensionsToolbarDesktop::GetDragOperationsForView(View* sender,
                                                       const gfx::Point& p) {
  return browser_->profile()->IsOffTheRecord() ? ui::DragDropTypes::DRAG_NONE
                                               : ui::DragDropTypes::DRAG_MOVE;
}

bool ExtensionsToolbarDesktop::CanStartDragForView(View* sender,
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
  auto it = std::ranges::find(
      toolbar_view_model_->GetPinnedActionIds(), sender,
      [this](const std::string& action_id) { return GetViewForId(action_id); });
  if (it == toolbar_view_model_->GetPinnedActionIds().cend()) {
    return false;
  }

  // TODO(crbug.com/40808374): Force-pinned extensions are not draggable.
  return !model_->IsActionForcePinned(*it);
}

std::unique_ptr<ExtensionActionViewModel>
ExtensionsToolbarDesktop::CreateActionViewModel(
    const ToolbarActionsModel::ActionId& action_id,
    ExtensionsContainer* extensions_container) {
  return ExtensionActionViewModel::Create(
      action_id, browser_,
      std::make_unique<ExtensionActionDelegateDesktop>(
          browser_.get(), extensions_container, this));
}

void ExtensionsToolbarDesktop::OnActionsInitialized() {
  CHECK(icons_.empty());

  for (const auto& action_id : toolbar_view_model_->GetAllActionIds()) {
    CreateActionViewForId(action_id);
  }

  ReorderAllChildViews();
  UpdateContainerVisibility();
}

void ExtensionsToolbarDesktop::OnActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  CreateActionViewForId(action_id);
  ReorderAllChildViews();

  // Auto hide mode should not become visible due to extensions being added,
  // only due to user interaction.
  if (display_mode_ != DisplayMode::kAutoHide) {
    UpdateContainerVisibility();
  }

  UpdateControlsVisibility();

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExtensionsToolbarDesktop::OnActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
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

void ExtensionsToolbarDesktop::OnActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewModel* action =
      toolbar_view_model_->GetActionModelForId(action_id);
  if (action) {
    ToolbarActionView* action_view = GetViewForId(action_id);
    action_view->UpdateState();
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

void ExtensionsToolbarDesktop::OnPinnedActionsChanged() {
  for (const auto& it : icons_) {
    UpdateIconVisibility(it.first);
  }
  ReorderAllChildViews();

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExtensionsToolbarDesktop::OnActiveWebContentsChanged(
    bool is_same_document) {
  content::WebContents* current_web_contents = GetCurrentWebContents();
  if (active_web_contents_.get() != current_web_contents) {
    // Tab switched
    BrowserUserEducationInterface::From(browser_)
        ->NotifyFeaturePromoFeatureUsed(
            feature_engagement::kIPHExtensionsMenuFeature,
            FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    if (request_access_button_) {
      CollapseConfirmation();
    }
  } else {
    // Navigation
    if (!is_same_document) {
      BrowserUserEducationInterface::From(browser_)->AbortFeaturePromo(
          feature_engagement::kIPHExtensionsMenuFeature);
    }
    if (request_access_button_ &&
        request_access_button_->IsShowingConfirmation() &&
        current_web_contents &&
        !request_access_button_->IsShowingConfirmationFor(
            current_web_contents->GetPrimaryMainFrame()
                ->GetLastCommittedOrigin())) {
      CollapseConfirmation();
    }
  }
  active_web_contents_ =
      current_web_contents ? current_web_contents->GetWeakPtr() : nullptr;

  UpdateAllIcons();
  MaybeShowIPH();
}

void ExtensionsToolbarDesktop::HideActivePopup() {
  if (popup_owner_) {
    popup_owner_->HidePopup();
  }
  DCHECK(!popup_owner_);
  UpdateContainerVisibilityAfterAnimation();
}

void ExtensionsToolbarDesktop::OnRequestAccessButtonParamsChanged(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  UpdateRequestAccessButton(*web_contents);
}

void ExtensionsToolbarDesktop::OnToolbarControlStateUpdated() {
  UpdateControlsVisibility();
}

bool ExtensionsToolbarDesktop::CloseOverflowMenuIfOpen() {
  if (IsExtensionsMenuShowing()) {
    HideExtensionsMenu();
    return true;
  }
  return false;
}

bool ExtensionsToolbarDesktop::CanShowToolbarActionPopupForAPICall(
    const ToolbarActionsModel::ActionId& action_id) {
  return !popped_out_action_ && browser_->window()->IsActive();
}

void ExtensionsToolbarDesktop::ToggleExtensionsMenu() {
  GetExtensionsButton()->ToggleExtensionsMenu();
}

bool ExtensionsToolbarDesktop::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return BrowserActionDragData::GetDropFormats(format_types);
}

bool ExtensionsToolbarDesktop::AreDropTypesRequired() {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool ExtensionsToolbarDesktop::CanDrop(const OSExchangeData& data) {
  return BrowserActionDragData::CanDrop(data, browser_->profile());
}

void ExtensionsToolbarDesktop::OnDragEntered(const ui::DropTargetEvent& event) {
  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

int ExtensionsToolbarDesktop::OnDragUpdated(const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data())) {
    return ui::DragDropTypes::DRAG_NONE;
  }

  // Check if there is an extension for the dragged icon (e.g. an extension can
  // be de deleted while dragging its icon).
  if (!toolbar_view_model_->GetActionModelForId(data.id())) {
    return ui::DragDropTypes::DRAG_NONE;
  }

  size_t before_icon = 0;
  // Figure out where to display the icon during dragging transition.

  // First, since we want to update the dragged extension's position from before
  // an icon to after it when the event passes the midpoint between two icons.
  // This will convert the event coordinate into the index of the icon we want
  // to display the dragged extension before. We also mirror the event.x() so
  // that our calculations are consistent with left-to-right.
  const int offset_into_icon_area = GetMirroredXInView(event.x());
  const size_t before_icon_unclamped = WidthToIconCount(offset_into_icon_area);

  const size_t visible_icons = toolbar_view_model_->GetPinnedActionIds().size();

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

void ExtensionsToolbarDesktop::OnDragExited() {
  if (!drop_info_) {
    return;
  }

  const ToolbarActionsModel::ActionId dragged_extension_id =
      drop_info_->action_id;
  drop_info_.reset();
  DragDropCleanup(dragged_extension_id);
}

views::View::DropCallback ExtensionsToolbarDesktop::GetDropCallback(
    const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data())) {
    return base::NullCallback();
  }

  auto action_id = std::move(drop_info_->action_id);
  auto index = drop_info_->index;
  drop_info_.reset();
  base::ScopedClosureRunner cleanup(
      base::BindOnce(&ExtensionsToolbarDesktop::DragDropCleanup,
                     weak_ptr_factory_.GetWeakPtr(), action_id));
  return base::BindOnce(&ExtensionsToolbarDesktop::MovePinnedAction,
                        drop_weak_ptr_factory_.GetWeakPtr(), action_id, index,
                        std::move(cleanup));
}

void ExtensionsToolbarDesktop::OnWidgetDestroying(views::Widget* widget) {
  auto iter =
      std::ranges::find(anchored_widgets_, widget, &AnchoredWidget::widget);
  CHECK(iter != anchored_widgets_.end());
  iter->widget->RemoveObserver(this);
  const std::string extension_id = std::move(iter->extension_id);
  anchored_widgets_.erase(iter);
  if (GetWidget() && !GetWidget()->IsClosed()) {
    UpdateIconVisibility(extension_id);
  }
}

size_t ExtensionsToolbarDesktop::WidthToIconCount(int x_offset) {
  const int element_padding =
      GetLayoutConstant(LayoutConstant::kToolbarElementPadding);
  size_t unclamped_count =
      std::max((x_offset + element_padding) /
                   (GetToolbarActionSize().width() + element_padding),
               0);
  return std::min(unclamped_count,
                  toolbar_view_model_->GetAllActionIds().size());
}

ui::ImageModel ExtensionsToolbarDesktop::GetExtensionIcon(
    ToolbarActionView* extension_view) {
  return extension_view->view_model()->GetIcon(GetCurrentWebContents(),
                                               GetToolbarActionSize());
}

void ExtensionsToolbarDesktop::SetExtensionIconVisibility(
    ToolbarActionsModel::ActionId id,
    bool visible) {
  auto it = std::ranges::find(
      toolbar_view_model_->GetPinnedActionIds(), GetViewForId(id),
      [this](const std::string& action_id) { return GetViewForId(action_id); });
  if (it == toolbar_view_model_->GetPinnedActionIds().cend()) {
    return;
  }

  ToolbarActionView* extension_view = GetViewForId(*it);
  if (!extension_view) {
    return;
  }

  extension_view->SetImageModel(
      views::Button::STATE_NORMAL,
      visible ? GetExtensionIcon(extension_view) : ui::ImageModel());
}

void ExtensionsToolbarDesktop::UpdateContainerVisibility() {
  bool was_visible = GetVisible();
  SetVisible(ShouldContainerBeVisible());

  // Layout animation does not handle host view visibility changing; requires
  // resetting.
  if (was_visible != GetVisible()) {
    GetAnimatingLayoutManager()->ResetLayout();
  }

  if (!was_visible && GetVisible() && GetOnVisibleCallbackForTesting()) {
    std::move(GetOnVisibleCallbackForTesting()).Run();
  }
}

bool ExtensionsToolbarDesktop::ShouldContainerBeVisible() const {
  // The container (and extensions-menu button) should not be visible if we have
  // no extensions.
  if (!toolbar_view_model_->HasAnyExtensions()) {
    return false;
  }

  // All other display modes are constantly visible.
  if (display_mode_ != DisplayMode::kAutoHide) {
    return true;
  }

  if (GetAnimatingLayoutManager()->is_animating()) {
    return true;
  }

  // Is menu showing.
  if (GetExtensionsButton()->GetExtensionsMenuShowing()) {
    return true;
  }

  // Is extension pop out is showing.
  if (popped_out_action_) {
    return true;
  }

  // Is extension pop up showing.
  if (popup_owner_) {
    return true;
  }

  return false;
}

void ExtensionsToolbarDesktop::UpdateContainerVisibilityAfterAnimation() {
  GetAnimatingLayoutManager()->PostOrQueueAction(
      base::BindOnce(&ExtensionsToolbarDesktop::UpdateContainerVisibility,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionsToolbarDesktop::OnMenuOpening() {
  content::WebContents* web_contents = GetCurrentWebContents();
  if (!web_contents) {
    return;
  }

  // Record IPH usage, which should only be shown when any extension has access.
  if (toolbar_view_model_->GetButtonState(*web_contents) ==
      ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
          kAnyExtensionHasAccess) {
    BrowserUserEducationInterface::From(browser_)
        ->NotifyFeaturePromoFeatureUsed(
            feature_engagement::kIPHExtensionsMenuFeature,
            FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  } else {
    // Otherwise, just close the IPH if it's present.
    BrowserUserEducationInterface::From(browser_)->AbortFeaturePromo(
        feature_engagement::kIPHExtensionsMenuFeature);
  }

  UpdateContainerVisibility();
}

void ExtensionsToolbarDesktop::OnMenuClosed() {
  UpdateContainerVisibility();
}

void ExtensionsToolbarDesktop::UpdateSidePanelState(bool is_active) {
  close_side_panel_button_->SetVisible(is_active);
  if (is_active) {
    close_side_panel_button_anchor_highlight_ =
        close_side_panel_button_->AddAnchorHighlight();
  } else {
    close_side_panel_button_anchor_highlight_.reset();
  }
}

void ExtensionsToolbarDesktop::MovePinnedAction(
    const ToolbarActionsModel::ActionId& action_id,
    size_t index,
    base::ScopedClosureRunner cleanup,
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  toolbar_view_model_->MovePinnedAction(action_id, index);

  output_drag_op = DragOperation::kMove;
  // `cleanup` will run automatically when it goes out of scope to finish
  // up the drag.
}

void ExtensionsToolbarDesktop::DragDropCleanup(
    const ToolbarActionsModel::ActionId& dragged_extension_id) {
  ReorderAllChildViews();
  GetAnimatingLayoutManager()->PostOrQueueAction(base::BindOnce(
      &ExtensionsToolbarDesktop::SetExtensionIconVisibility,
      weak_ptr_factory_.GetWeakPtr(), dragged_extension_id, true));
}

void ExtensionsToolbarDesktop::UpdateControlsVisibility() {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    return;
  }

  content::WebContents* web_contents = GetCurrentWebContents();
  if (!web_contents) {
    return;
  }

  UpdateExtensionsButton(*web_contents);
  UpdateRequestAccessButton(*web_contents);
}

void ExtensionsToolbarDesktop::CloseSidePanelButtonPressed() {
  browser_->GetFeatures().side_panel_ui()->Close(
      extensions::ExtensionSidePanelCoordinator::GetPanelType());
}

void ExtensionsToolbarDesktop::CollapseConfirmation() {
  if (!request_access_button_->IsShowingConfirmation()) {
    return;
  }

  request_access_button_->ResetConfirmation();
  UpdateControlsVisibility();
}

void ExtensionsToolbarDesktop::ShowContextMenuAsFallback(
    const extensions::ExtensionId& action_id) {
  GetViewForId(action_id)->ShowContextMenuAsFallback();
}

void ExtensionsToolbarDesktop::OnPopupShown(
    const extensions::ExtensionId& action_id,
    bool by_user) {
  GetViewForId(action_id)->OnPopupShown(by_user);
}

void ExtensionsToolbarDesktop::OnPopupClosed(
    const extensions::ExtensionId& action_id) {
  GetViewForId(action_id)->OnPopupClosed();
}

views::FocusManager* ExtensionsToolbarDesktop::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::BubbleAnchor ExtensionsToolbarDesktop::GetReferenceButtonForPopup(
    const extensions::ExtensionId& action_id) {
  return GetViewForId(action_id)->GetReferenceButtonForPopup();
}

void ExtensionsToolbarDesktop::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHoverCard(nullptr, ToolbarActionHoverCardUpdateType::kHover);
}

void ExtensionsToolbarDesktop::OnMouseMoved(const ui::MouseEvent& event) {
  // Since we set the container's "notify enter exit on child" to true, we can
  // get notified when the mouse enters a child view only if it originates from
  // outside the container. This means that we a) can know when the mouse enters
  // a toolbar action view (which is handled in such class) and b) cannot
  // know when the mouse leaves a toolbar action view and enters a toolbar
  // control. Therefore, listening for on mouse moved in the container reflects
  // moving the mouse from toolbar action view to toolbar controls.
  UpdateHoverCard(nullptr, ToolbarActionHoverCardUpdateType::kHover);
}

void ExtensionsToolbarDesktop::UpdateCloseSidePanelButtonIcon() {
  const bool is_right_aligned = browser_->profile()->GetPrefs()->GetBoolean(
      prefs::kSidePanelHorizontalAlignment);
  close_side_panel_button_->SetVectorIcon(
      is_right_aligned ? kRightPanelCloseIcon : kLeftPanelCloseIcon);
}

void ExtensionsToolbarDesktop::MaybeShowIPH() {
  // Extensions menu IPH, with priority order. These depend on the new access
  // control feature.
  content::WebContents* web_contents = GetCurrentWebContents();
  if (!web_contents) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    ExtensionsRequestAccessButton* request_access_button =
        GetRequestAccessButton();
    if (request_access_button && request_access_button->GetVisible()) {
      const int extensions_size = request_access_button->GetExtensionsCount();
      user_education::FeaturePromoParams params(
          feature_engagement::kIPHExtensionsRequestAccessButtonFeature);
      params.body_params = extensions_size;
      params.title_params = extensions_size;
      BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
          std::move(params));
    }

    if (toolbar_view_model_->GetButtonState(*web_contents) ==
        ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
            kAnyExtensionHasAccess) {
      BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
          feature_engagement::kIPHExtensionsMenuFeature);
    }
  }

  // The Extensions Zero State promo prompts users without extensions to
  // explore the Chrome Web Store. Only triggered for normal browser types.
  if (browser_->type() == Browser::TYPE_NORMAL) {
    if (!g_zero_state_promo_next_show_time_opt.has_value()) {
      g_zero_state_promo_next_show_time_opt =
          base::TimeTicks::Now() + kZeroStatePromoIntervalBetweenLaunchAttempt;
    } else if (base::TimeTicks::Now() >=
                   g_zero_state_promo_next_show_time_opt.value() &&
               ArePromotionsEnabled() &&
               !extensions::util::AnyCurrentlyInstalledExtensionIsFromWebstore(
                   browser_->profile())) {
      g_zero_state_promo_next_show_time_opt =
          base::TimeTicks::Now() + kZeroStatePromoIntervalBetweenLaunchAttempt;
      BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
          feature_engagement::kIPHExtensionsZeroStatePromoFeature);
    }
  }
}

BEGIN_METADATA(ExtensionsToolbarDesktop)
END_METADATA
