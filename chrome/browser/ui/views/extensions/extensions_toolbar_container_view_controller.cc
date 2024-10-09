// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container_view_controller.h"

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "extensions/common/extension_features.h"

ExtensionsToolbarContainerViewController::
    ExtensionsToolbarContainerViewController(
        Browser* browser,
        ExtensionsToolbarContainer* extensions_container)
    : browser_(browser), extensions_container_(extensions_container) {
  model_observation_.Observe(ToolbarActionsModel::Get(browser_->profile()));
  permissions_manager_observation_.Observe(
      extensions::PermissionsManager::Get(browser_->profile()));
  browser_->tab_strip_model()->AddObserver(this);
}

ExtensionsToolbarContainerViewController::
    ~ExtensionsToolbarContainerViewController() {
  extensions_container_ = nullptr;
  model_observation_.Reset();
  permissions_manager_observation_.Reset();
}

void ExtensionsToolbarContainerViewController::
    WindowControlsOverlayEnabledChanged(bool enabled) {
  if (!extensions_container_->main_item()) {
    return;
  }

  extensions_container_->UpdateContainerVisibility();
  extensions_container_->main_item()->ClearProperty(views::kFlexBehaviorKey);

  views::MinimumFlexSizeRule min_flex_rule =
      enabled ? views::MinimumFlexSizeRule::kPreferred
              : views::MinimumFlexSizeRule::kPreferredSnapToZero;
  extensions_container_->main_item()->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(min_flex_rule,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(ExtensionsToolbarContainerViewController::
                         kFlexOrderExtensionsButton));
}

void ExtensionsToolbarContainerViewController::MaybeShowIPH() {
  // IPH is only shown for the kExtensionsMenuAccessControl feature.
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    return;
  }

  CHECK(browser_->window());

  // Display IPH, with priority order.
  ExtensionsRequestAccessButton* request_access_button =
      extensions_container_->GetRequestAccessButton();
  if (request_access_button->GetVisible()) {
    const int extensions_size = request_access_button->GetExtensionsCount();
    user_education::FeaturePromoParams params(
        feature_engagement::kIPHExtensionsRequestAccessButtonFeature);
    params.body_params = extensions_size;
    params.title_params = extensions_size;
    browser_->window()->MaybeShowFeaturePromo(std::move(params));
  }

  if (extensions_container_->GetExtensionsButton()->state() ==
      ExtensionsToolbarButton::State::kAnyExtensionHasAccess) {
    browser_->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHExtensionsMenuFeature);
  }
}

void ExtensionsToolbarContainerViewController::UpdateRequestAccessButton() {
  CHECK(extensions_container_);

  auto* web_contents = extensions_container_->GetCurrentWebContents();
  extensions::PermissionsManager::UserSiteSetting site_setting =
      extensions::PermissionsManager::Get(browser_->profile())
          ->GetUserSiteSetting(
              web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  extensions_container_->UpdateRequestAccessButton(site_setting, web_contents);
}

void ExtensionsToolbarContainerViewController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->empty() || !selection.active_tab_changed()) {
    return;
  }

  // Close Extensions menu IPH if it is open.
  browser_->window()->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHExtensionsMenuFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);

  extensions::MaybeShowExtensionControlledNewTabPage(browser_,
                                                     selection.new_contents);

  // Request access button confirmation is tab-specific. Therefore, we need to
  // reset if the active tab changes.
  if (extensions_container_->GetRequestAccessButton()) {
    extensions_container_->CollapseConfirmation();
  }

  MaybeShowIPH();
}

void ExtensionsToolbarContainerViewController::TabChangedAt(
    content::WebContents* contents,
    int index,
    TabChangeType change_type) {
  // Ignore changes that don't affect all the tab contents (e.g loading
  // changes).
  if (change_type != TabChangeType::kAll) {
    return;
  }

  // Close Extensions menu IPH if it is open.
  browser_->window()->AbortFeaturePromo(
      feature_engagement::kIPHExtensionsMenuFeature);

  // Request access button confirmation is tab-specific for a specific origin.
  // Therefore, we need to reset it if it's currently showing, we are on the
  // same tab and we have navigated to another origin.
  // Note: When we switch tabs, `OnTabStripModelChanged` is called before
  // `TabChangedAt` and takes care of resetting the confirmation if shown.
  ExtensionsRequestAccessButton* request_access_button =
      extensions_container_->GetRequestAccessButton();
  if (request_access_button && request_access_button->IsShowingConfirmation() &&
      !request_access_button->IsShowingConfirmationFor(
          contents->GetPrimaryMainFrame()->GetLastCommittedOrigin())) {
    extensions_container_->CollapseConfirmation();
  }

  MaybeShowIPH();
}

void ExtensionsToolbarContainerViewController::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  CHECK(extensions_container_);
  extensions_container_->AddAction(action_id);
}

void ExtensionsToolbarContainerViewController::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  CHECK(extensions_container_);
  extensions_container_->RemoveAction(action_id);
}

void ExtensionsToolbarContainerViewController::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  CHECK(extensions_container_);
  extensions_container_->UpdateAction(action_id);
}

void ExtensionsToolbarContainerViewController::OnToolbarModelInitialized() {
  CHECK(extensions_container_);
  extensions_container_->CreateActions();
}

void ExtensionsToolbarContainerViewController::OnToolbarPinnedActionsChanged() {
  CHECK(extensions_container_);
  extensions_container_->UpdatePinnedActions();
}

void ExtensionsToolbarContainerViewController::OnUserPermissionsSettingsChanged(
    const extensions::PermissionsManager::UserPermissionsSettings& settings) {
  CHECK(extensions_container_);
  extensions_container_->UpdateControlsVisibility();
  // TODO(crbug.com/40857356): Update request access button hover card. This
  // will be slightly different than 'OnToolbarActionUpdated' since site
  // settings update are not tied to a specific action.
}

void ExtensionsToolbarContainerViewController::
    OnShowAccessRequestsInToolbarChanged(
        const extensions::ExtensionId& extension_id,
        bool can_show_requests) {
  CHECK(extensions_container_);
  extensions_container_->UpdateControlsVisibility();
  // TODO(crbug.com/40857356): Update requests access button hover card. This is
  // tricky because it would need to change the items in the dialog. Another
  // option is to close the hover card if its shown whenever request access
  // button is updated.
}

void ExtensionsToolbarContainerViewController::
    OnSiteAccessRequestDismissedByUser(
        const extensions::ExtensionId& extension_id,
        const url::Origin& origin) {
  UpdateRequestAccessButton();
}

void ExtensionsToolbarContainerViewController::OnSiteAccessRequestAdded(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  int current_tab_id = extensions::ExtensionTabUtil::GetTabId(
      extensions_container_->GetCurrentWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  UpdateRequestAccessButton();
}

void ExtensionsToolbarContainerViewController::OnSiteAccessRequestUpdated(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  UpdateRequestAccessButton();
}

void ExtensionsToolbarContainerViewController::OnSiteAccessRequestRemoved(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  int current_tab_id = extensions::ExtensionTabUtil::GetTabId(
      extensions_container_->GetCurrentWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  UpdateRequestAccessButton();
}

void ExtensionsToolbarContainerViewController::OnSiteAccessRequestsCleared(
    int tab_id) {
  int current_tab_id = extensions::ExtensionTabUtil::GetTabId(
      extensions_container_->GetCurrentWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  UpdateRequestAccessButton();
}
