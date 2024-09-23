// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_id.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

// Returns the extension for `extension_id`.
const extensions::Extension* GetExtension(
    Browser* browser,
    extensions::ExtensionId extension_id) {
  return extensions::ExtensionRegistry::Get(browser->profile())
      ->enabled_extensions()
      .GetByID(extension_id);
}

// Returns sorted extension ids based on their extensions name.
std::vector<std::string> SortExtensionsByName(
    ToolbarActionsModel& toolbar_model) {
  auto sort_by_name = [&toolbar_model](const ToolbarActionsModel::ActionId a,
                                       const ToolbarActionsModel::ActionId b) {
    return base::i18n::ToLower(toolbar_model.GetExtensionName(a)) <
           base::i18n::ToLower(toolbar_model.GetExtensionName(b));
  };
  std::vector<std::string> sorted_ids(toolbar_model.action_ids().begin(),
                                      toolbar_model.action_ids().end());
  std::sort(sorted_ids.begin(), sorted_ids.end(), sort_by_name);
  return sorted_ids;
}

// Returns the index of `action_id` in the toolbar model actions based on the
// extensions name alphabetical order.
size_t FindIndex(ToolbarActionsModel& toolbar_model,
                 const ToolbarActionsModel::ActionId& action_id) {
  std::u16string extension_name =
      base::i18n::ToLower(toolbar_model.GetExtensionName(action_id));
  auto sorted_action_ids = SortExtensionsByName(toolbar_model);
  return static_cast<size_t>(
      base::ranges::lower_bound(sorted_action_ids, extension_name, {},
                                [&toolbar_model](std::string id) {
                                  return base::i18n::ToLower(
                                      toolbar_model.GetExtensionName(id));
                                }) -
      sorted_action_ids.begin());
}

// Returns the main page, if it is the correct type.
ExtensionsMenuMainPageView* GetMainPage(views::View* page) {
  return views::AsViewClass<ExtensionsMenuMainPageView>(page);
}

// Returns the site permissions page, if it is the correct type.
ExtensionsMenuSitePermissionsPageView* GetSitePermissionsPage(
    views::View* page) {
  return views::AsViewClass<ExtensionsMenuSitePermissionsPageView>(page);
}

// Returns whether `extension` cannot have its site access modified by the user
// because of policy.
bool HasEnterpriseForcedAccess(const extensions::Extension& extension,
                               Profile& profile) {
  extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(&profile)->management_policy();
  return !policy->UserMayModifySettings(&extension, nullptr) ||
         policy->MustRemainInstalled(&extension, nullptr);
}

// Returns whether the site setting toggle for `web_contents` should be visible.
bool IsSiteSettingsToggleVisible(const ToolbarActionsModel& toolbar_model,
                                 content::WebContents* web_contents) {
  const GURL& url = web_contents->GetLastCommittedURL();
  return !toolbar_model.IsRestrictedUrl(url) &&
         !toolbar_model.IsPolicyBlockedHost(url);
}

// Returns whether the site settings toggle for `web_contents` should be on.
bool IsSiteSettingsToggleOn(Browser* browser,
                            content::WebContents* web_contents) {
  auto origin = web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  return PermissionsManager::Get(browser->profile())
             ->GetUserSiteSetting(origin) ==
         PermissionsManager::UserSiteSetting::kCustomizeByExtension;
}

// Returns whether the site permissions button should be visible.
bool IsSitePermissionsButtonVisible(const extensions::Extension& extension,
                                    Profile& profile,
                                    const ToolbarActionsModel& toolbar_model,
                                    content::WebContents& web_contents) {
  // Button is never visible when site is restricted.
  if (toolbar_model.IsRestrictedUrl(web_contents.GetLastCommittedURL())) {
    return false;
  }

  PermissionsManager::UserSiteSetting user_site_setting =
      PermissionsManager::Get(&profile)->GetUserSiteSetting(
          web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin());
  switch (user_site_setting) {
    case PermissionsManager::UserSiteSetting::kCustomizeByExtension: {
      // Extensions should always display the button.
      return true;
    }
    case PermissionsManager::UserSiteSetting::kBlockAllExtensions: {
      // Extension should only display the button when it's an enterprise
      // extension and has granted access.
      bool enterprise_forced_access =
          HasEnterpriseForcedAccess(extension, profile);
      SitePermissionsHelper::SiteInteraction site_interaction =
          SitePermissionsHelper(&profile).GetSiteInteraction(extension,
                                                             &web_contents);
      return enterprise_forced_access &&
             site_interaction ==
                 SitePermissionsHelper::SiteInteraction::kGranted;
    }
    case PermissionsManager::UserSiteSetting::kGrantAllExtensions: {
      NOTREACHED();
    }
  }
}

// Returns whether user can select the site access for `extension` on
// `web_contents`.
bool CanUserCustomizeExtensionSiteAccess(
    const extensions::Extension& extension,
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents) {
  const GURL& url = web_contents.GetLastCommittedURL();
  if (toolbar_model.IsRestrictedUrl(url)) {
    // We don't allow customization of restricted sites (e.g.
    // chrome://settings).
    return false;
  }

  if (extension.permissions_data()->IsPolicyBlockedHost(url)) {
    // Users can't customize the site access of policy-blocked sites.
    return false;
  }

  if (HasEnterpriseForcedAccess(extension, profile)) {
    // Users can't customize the site access of enterprise-installed extensions.
    return false;
  }

  // The extension wants site access if it at least wants "on click" access.
  auto* permissions_manager = PermissionsManager::Get(&profile);
  bool extension_wants_access = permissions_manager->CanUserSelectSiteAccess(
      extension, url, PermissionsManager::UserSiteAccess::kOnClick);
  if (!extension_wants_access) {
    // Users can't customize site access of extensions that don't want access to
    // begin with.
    return false;
  }

  // Users can only customize site access when they have allowed all extensions
  // to be customizable on the site.
  return permissions_manager->GetUserSiteSetting(
             web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin()) ==
         PermissionsManager::UserSiteSetting::kCustomizeByExtension;
}

// Returns the state for the `extension`'s site permissions button.
ExtensionMenuItemView::SitePermissionsButtonState GetSitePermissionsButtonState(
    const extensions::Extension& extension,
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents) {
  bool is_site_permissions_button_visible = IsSitePermissionsButtonVisible(
      extension, profile, toolbar_model, web_contents);
  if (!is_site_permissions_button_visible) {
    return ExtensionMenuItemView::SitePermissionsButtonState::kHidden;
  }

  bool is_site_permissions_button_enabled = CanUserCustomizeExtensionSiteAccess(
      extension, profile, toolbar_model, web_contents);
  return is_site_permissions_button_enabled
             ? ExtensionMenuItemView::SitePermissionsButtonState::kEnabled
             : ExtensionMenuItemView::SitePermissionsButtonState::kDisabled;
}

// Returns the sites access displayed by the `extension`'s site permissions
// button.
ExtensionMenuItemView::SitePermissionsButtonAccess
GetSitePermissionsButtonAccess(const extensions::Extension& extension,
                               Profile& profile,
                               const ToolbarActionsModel& toolbar_model,
                               content::WebContents& web_contents) {
  auto site_interaction = SitePermissionsHelper(&profile).GetSiteInteraction(
      extension, &web_contents);
  if (site_interaction == SitePermissionsHelper::SiteInteraction::kNone) {
    return ExtensionMenuItemView::SitePermissionsButtonAccess::kNone;
  }

  auto site_access = PermissionsManager::Get(&profile)->GetUserSiteAccess(
      extension, web_contents.GetLastCommittedURL());
  switch (site_access) {
    case PermissionsManager::UserSiteAccess::kOnClick:
      return ExtensionMenuItemView::SitePermissionsButtonAccess::kOnClick;
    case PermissionsManager::UserSiteAccess::kOnSite:
      return ExtensionMenuItemView::SitePermissionsButtonAccess::kOnSite;
    case PermissionsManager::UserSiteAccess::kOnAllSites:
      return ExtensionMenuItemView::SitePermissionsButtonAccess::kOnAllSites;
  }
}

// Returns the state for the `extension`'s site access toggle button.
ExtensionMenuItemView::SiteAccessToggleState GetSiteAccessToggleState(
    const extensions::Extension& extension,
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents) {
  if (!CanUserCustomizeExtensionSiteAccess(extension, profile, toolbar_model,
                                           web_contents)) {
    return ExtensionMenuItemView::SiteAccessToggleState::kHidden;
  }

  // Button is on iff the extension has access to the site.
  auto site_interaction = SitePermissionsHelper(&profile).GetSiteInteraction(
      extension, &web_contents);
  return site_interaction == SitePermissionsHelper::SiteInteraction::kGranted
             ? ExtensionMenuItemView::SiteAccessToggleState::kOn
             : ExtensionMenuItemView::SiteAccessToggleState::kOff;
}

// Returns the state for the message section in the menu.
ExtensionsMenuMainPageView::MessageSectionState GetMessageSectionState(
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents) {
  const GURL& url = web_contents.GetLastCommittedURL();
  if (toolbar_model.IsRestrictedUrl(url)) {
    return ExtensionsMenuMainPageView::MessageSectionState::kRestrictedAccess;
  }

  if (toolbar_model.IsPolicyBlockedHost(url)) {
    return ExtensionsMenuMainPageView::MessageSectionState::
        kPolicyBlockedAccess;
  }

  PermissionsManager::UserSiteSetting site_setting =
      PermissionsManager::Get(&profile)->GetUserSiteSetting(
          web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin());
  bool reload_required =
      extensions::TabHelper::FromWebContents(&web_contents)->IsReloadRequired();

  if (site_setting ==
      PermissionsManager::UserSiteSetting::kBlockAllExtensions) {
    return reload_required ? ExtensionsMenuMainPageView::MessageSectionState::
                                 kUserBlockedAccessReload
                           : ExtensionsMenuMainPageView::MessageSectionState::
                                 kUserBlockedAccess;
  }

  return reload_required ? ExtensionsMenuMainPageView::MessageSectionState::
                               kUserCustomizedAccessReload
                         : ExtensionsMenuMainPageView::MessageSectionState::
                               kUserCustomizedAccess;
}

void LogSiteAccessUpdate(PermissionsManager::UserSiteAccess site_access) {
  switch (site_access) {
    case PermissionsManager::UserSiteAccess::kOnClick:
      base::RecordAction(
          base::UserMetricsAction("Extensions.Menu.OnClickSelected"));
      break;
    case PermissionsManager::UserSiteAccess::kOnSite:
      base::RecordAction(
          base::UserMetricsAction("Extensions.Menu.OnSiteSelected"));
      break;
    case PermissionsManager::UserSiteAccess::kOnAllSites:
      base::RecordAction(
          base::UserMetricsAction("Extensions.Menu.OnAllSitesSelected"));
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown site access";
      break;
  }
}

}  // namespace

ExtensionsMenuViewController::ExtensionsMenuViewController(
    Browser* browser,
    ExtensionsContainer* extensions_container,
    views::View* bubble_contents,
    views::BubbleDialogDelegate* bubble_delegate)
    : browser_(browser),
      extensions_container_(extensions_container),
      bubble_contents_(bubble_contents),
      bubble_delegate_(bubble_delegate),
      toolbar_model_(ToolbarActionsModel::Get(browser_->profile())) {
  browser_->tab_strip_model()->AddObserver(this);
  toolbar_model_observation_.Observe(toolbar_model_.get());
  permissions_manager_observation_.Observe(
      PermissionsManager::Get(browser_->profile()));
}

ExtensionsMenuViewController::~ExtensionsMenuViewController() {
  // Note: No need to call TabStripModel::RemoveObserver(), because it's handled
  // directly within TabStripModelObserver::~TabStripModelObserver().
}

void ExtensionsMenuViewController::OpenMainPage() {
  auto main_page = std::make_unique<ExtensionsMenuMainPageView>(browser_, this);
  UpdateMainPage(main_page.get(), GetActiveWebContents());
  PopulateMainPage(main_page.get());

  SwitchToPage(std::move(main_page));
}

void ExtensionsMenuViewController::OpenSitePermissionsPage(
    const extensions::ExtensionId& extension_id) {
  CHECK(CanUserCustomizeExtensionSiteAccess(
      *GetExtension(browser_, extension_id), *browser_->profile(),
      *toolbar_model_, *GetActiveWebContents()));

  auto site_permissions_page =
      std::make_unique<ExtensionsMenuSitePermissionsPageView>(
          browser_, extension_id, this);
  UpdateSitePermissionsPage(site_permissions_page.get(),
                            GetActiveWebContents());

  SwitchToPage(std::move(site_permissions_page));

  base::RecordAction(
      base::UserMetricsAction("Extensions.Menu.SitePermissionsPageOpened"));
}

void ExtensionsMenuViewController::CloseBubble() {
  bubble_contents_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void ExtensionsMenuViewController::OnSiteAccessSelected(
    const extensions::ExtensionId& extension_id,
    PermissionsManager::UserSiteAccess site_access) {
  LogSiteAccessUpdate(site_access);

  SitePermissionsHelper permissions(browser_->profile());
  permissions.UpdateSiteAccess(*GetExtension(browser_, extension_id),
                               GetActiveWebContents(), site_access);
}

void ExtensionsMenuViewController::OnSiteSettingsToggleButtonPressed(
    bool is_on) {
  content::WebContents* web_contents = GetActiveWebContents();
  const url::Origin& origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  PermissionsManager::UserSiteSetting site_setting =
      is_on ? PermissionsManager::UserSiteSetting::kCustomizeByExtension
            : PermissionsManager::UserSiteSetting::kBlockAllExtensions;

  extensions::TabHelper::FromWebContents(web_contents)
      ->SetReloadRequired(site_setting);
  PermissionsManager::Get(browser_->profile())
      ->UpdateUserSiteSetting(origin, site_setting);

  if (is_on) {
    base::RecordAction(
        base::UserMetricsAction("Extensions.Menu.AllowByExtensionSelected"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Extensions.Menu.ExtensionsBlockedSelected"));
  }
}

void ExtensionsMenuViewController::OnExtensionToggleSelected(
    const extensions::ExtensionId& extension_id,
    bool is_on) {
  const extensions::Extension* extension = GetExtension(browser_, extension_id);
  content::WebContents* web_contents = GetActiveWebContents();
  CHECK(CanUserCustomizeExtensionSiteAccess(*extension, *browser_->profile(),
                                            *toolbar_model_, *web_contents));

  SitePermissionsHelper permissions_helper(browser_->profile());
  auto* permissions_manager = PermissionsManager::Get(browser_->profile());
  auto current_site_access = permissions_manager->GetUserSiteAccess(
      *extension, web_contents->GetLastCommittedURL());
  PermissionsManager::ExtensionSiteAccess extension_site_access =
      permissions_manager->GetSiteAccess(*extension,
                                         web_contents->GetLastCommittedURL());

  // Grant extension site access when extension is toggled on.
  if (is_on) {
    DCHECK_EQ(current_site_access,
              PermissionsManager::UserSiteAccess::kOnClick);

    // Update site access when extension requested host permissions for the
    // current site (that is, site access was withheld).
    if (extension_site_access.withheld_site_access ||
        extension_site_access.withheld_all_sites_access) {
      // Restore to previous access by looking whether broad site access was
      // previously granted.
      PermissionsManager::UserSiteAccess new_site_access =
          permissions_manager->HasPreviousBroadSiteAccess(extension_id)
              ? PermissionsManager::UserSiteAccess::kOnAllSites
              : PermissionsManager::UserSiteAccess::kOnSite;
      permissions_helper.UpdateSiteAccess(*extension, web_contents,
                                          new_site_access);
      return;
    }

    // Otherwise, grant one-time access (e.g. extension with activeTab is
    // granted access).
    extensions::ExtensionActionRunner* action_runner =
        extensions::ExtensionActionRunner::GetForWebContents(web_contents);
    if (action_runner) {
      action_runner->GrantTabPermissions({extension});
    }
    return;
  }

  // Revoke extension's site access when extension is toggled off.

  // Update site access to "on click" when extension requested, and was granted,
  // host permissions for the current site (that is, extension has site access).
  if (extension_site_access.has_site_access ||
      extension_site_access.has_all_sites_access) {
    DCHECK_NE(current_site_access,
              PermissionsManager::UserSiteAccess::kOnClick);
    permissions_helper.UpdateSiteAccess(
        *extension, web_contents, PermissionsManager::UserSiteAccess::kOnClick);
    return;
  }

  // Otherwise, extension has one-time access and we need to clear tab
  // permissions (e.g extension with activeTab was granted one-time access).
  DCHECK_EQ(current_site_access, PermissionsManager::UserSiteAccess::kOnClick);
  extensions::TabHelper::FromWebContents(web_contents)
      ->active_tab_permission_granter()
      ->ClearActiveExtensionAndNotify(extension_id);

  auto* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  if (action_runner) {
    action_runner->ShowReloadPageBubble({extension_id});
  }
}

void ExtensionsMenuViewController::OnReloadPageButtonClicked() {
  GetActiveWebContents()->GetController().Reload(content::ReloadType::NORMAL,
                                                 false);
}

void ExtensionsMenuViewController::OnAllowExtensionClicked(
    const extensions::ExtensionId& extension_id) {
  content::WebContents* web_contents = GetActiveWebContents();
  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  if (!action_runner) {
    return;
  }

  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.ExtensionActivatedFromAllowingRequestAccessInMenu"));
  action_runner->GrantTabPermissions({GetExtension(browser_, extension_id)});
  // TODO(crbug.com/40912394): Granting tab permission but not accepting the
  // reload page means we grant tab permissions but the action is not executed.
  // This causes a mismatch between the request access button in the toolbar,
  // and the request access section in the menu when the extension is granted
  // tab permission by one item but the action is not run.
}

void ExtensionsMenuViewController::OnDismissExtensionClicked(
    const extensions::ExtensionId& extension_id) {
  auto* permissions_manager = PermissionsManager::Get(browser_->profile());
  CHECK(permissions_manager);
  content::WebContents* web_contents = GetActiveWebContents();
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  permissions_manager->UserDismissedSiteAccessRequest(web_contents, tab_id,
                                                      extension_id);

  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.ExtensionRequestDismissedFromMenu"));
}

void ExtensionsMenuViewController::OnShowRequestsTogglePressed(
    const extensions::ExtensionId& extension_id,
    bool is_on) {
  extensions::SitePermissionsHelper(browser_->profile())
      .SetShowAccessRequestsInToolbar(extension_id, is_on);

  if (is_on) {
    base::RecordAction(base::UserMetricsAction(
        "Extensions.Menu.ShowRequestsInToolbarPressed"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Extensions.Menu.HideRequestsInToolbarPressed"));
  }
}

void ExtensionsMenuViewController::TabChangedAt(content::WebContents* contents,
                                                int index,
                                                TabChangeType change_type) {
  bool should_update_page = false;
  switch (change_type) {
    case TabChangeType::kAll:
      should_update_page = true;
      break;
    case TabChangeType::kLoadingOnly:
      should_update_page = false;
      break;
  }

  if (!should_update_page || GetActiveWebContents() != contents) {
    return;
  }

  UpdatePage(contents);
}

void ExtensionsMenuViewController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  CHECK_EQ(tab_strip_model, browser_->tab_strip_model());
  content::WebContents* web_contents = GetActiveWebContents();

  if (!selection.active_tab_changed() || !web_contents) {
    return;
  }

  UpdatePage(web_contents);
}

void ExtensionsMenuViewController::UpdatePage(
    content::WebContents* web_contents) {
  DCHECK(current_page_);

  if (!web_contents) {
    return;
  }

  auto* site_permissions_page = GetSitePermissionsPage(current_page_.view());
  if (site_permissions_page) {
    // Update site permissions page if the extension can have one.
    if (CanUserCustomizeExtensionSiteAccess(
            *GetExtension(browser_, site_permissions_page->extension_id()),
            *browser_->profile(), *toolbar_model_, *web_contents)) {
      UpdateSitePermissionsPage(site_permissions_page, web_contents);
      return;
    }

    // Otherwise navigate back to the main page.
    OpenMainPage();
    return;
  }

  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  DCHECK(main_page);
  UpdateMainPage(main_page, web_contents);
}

void ExtensionsMenuViewController::UpdateMainPage(
    ExtensionsMenuMainPageView* main_page,
    content::WebContents* web_contents) {
  CHECK(web_contents);

  // Update site settings.
  std::u16string current_site = GetCurrentHost(web_contents);
  bool is_site_settings_toggle_visible =
      IsSiteSettingsToggleVisible(*toolbar_model_, web_contents);
  bool is_site_settings_toggle_on =
      IsSiteSettingsToggleOn(browser_, web_contents);
  main_page->UpdateSiteSettings(current_site, is_site_settings_toggle_visible,
                                is_site_settings_toggle_on);

  // Update message section.
  ExtensionsMenuMainPageView::MessageSectionState message_section_state =
      GetMessageSectionState(*browser_->profile(), *toolbar_model_,
                             *web_contents);
  bool has_enterprise_extensions = false;
  // Only kUserBlockedAccess or kPolicyBlockedAccess states care whether there
  // are any extensions installed by enterprise.
  if (message_section_state ==
          ExtensionsMenuMainPageView::MessageSectionState::kUserBlockedAccess ||
      message_section_state == ExtensionsMenuMainPageView::MessageSectionState::
                                   kPolicyBlockedAccess) {
    has_enterprise_extensions = std::any_of(
        toolbar_model_->action_ids().begin(),
        toolbar_model_->action_ids().end(),
        [this](const ToolbarActionsModel::ActionId extension_id) {
          auto* extension = GetExtension(browser_, extension_id);
          return HasEnterpriseForcedAccess(*extension, *browser_->profile());
        });
  }
  main_page->UpdateMessageSection(message_section_state,
                                  has_enterprise_extensions);

  if (message_section_state ==
      ExtensionsMenuMainPageView::MessageSectionState::kUserCustomizedAccess) {
    int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
    auto* permissions_manager = PermissionsManager::Get(browser_->profile());
    int index = 0;
    std::vector<std::string> extension_ids =
        SortExtensionsByName(*toolbar_model_);

    for (const auto& extension_id : extension_ids) {
      if (permissions_manager->HasActiveSiteAccessRequest(tab_id,
                                                          extension_id)) {
        AddOrUpdateExtensionRequestingAccess(main_page, extension_id, index,
                                             web_contents);
        ++index;
      } else {
        // Otherwise remove its entry, if existent.
        main_page->RemoveExtensionRequestingAccess(extension_id);
      }
    }
  }

  // Update menu items.
  // TODO(crbug.com/40879945): Reorder the extensions after updating them, since
  // their names can change.
  std::vector<ExtensionMenuItemView*> menu_items = main_page->GetMenuItems();
  for (auto* menu_item : menu_items) {
    const extensions::Extension* extension =
        GetExtension(browser_, menu_item->view_controller()->GetId());
    CHECK(extension);

    ExtensionMenuItemView::SiteAccessToggleState site_access_toggle_state =
        GetSiteAccessToggleState(*extension, *browser_->profile(),
                                 *toolbar_model_, *web_contents);
    ExtensionMenuItemView::SitePermissionsButtonState
        site_permissions_button_state = GetSitePermissionsButtonState(
            *extension, *browser_->profile(), *toolbar_model_, *web_contents);
    ExtensionMenuItemView::SitePermissionsButtonAccess
        site_permissions_button_access = GetSitePermissionsButtonAccess(
            *extension, *browser_->profile(), *toolbar_model_, *web_contents);
    bool is_enterprise =
        HasEnterpriseForcedAccess(*extension, *browser_->profile());
    menu_item->Update(site_access_toggle_state, site_permissions_button_state,
                      site_permissions_button_access, is_enterprise);
  }
}

void ExtensionsMenuViewController::UpdateSitePermissionsPage(
    ExtensionsMenuSitePermissionsPageView* site_permissions_page,
    content::WebContents* web_contents) {
  CHECK(web_contents);

  auto* permissions_manager = PermissionsManager::Get(browser_->profile());
  SitePermissionsHelper permissions_helper(browser_->profile());

  extensions::ExtensionId extension_id = site_permissions_page->extension_id();
  const extensions::Extension* extension = GetExtension(browser_, extension_id);
  const GURL& url = web_contents->GetLastCommittedURL();
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  ToolbarActionViewController* action_controller =
      extensions_container_->GetActionForId(extension_id);

  std::u16string extension_name = action_controller->GetActionName();
  ui::ImageModel extension_icon =
      action_controller->GetIcon(web_contents, gfx::Size(icon_size, icon_size));
  std::u16string current_site = GetCurrentHost(web_contents);
  PermissionsManager::UserSiteAccess user_site_access =
      permissions_manager->GetUserSiteAccess(*extension, url);
  bool is_show_requests_toggle_on =
      permissions_helper.ShowAccessRequestsInToolbar(extension_id);
  bool is_on_site_enabled = permissions_manager->CanUserSelectSiteAccess(
      *extension, url, PermissionsManager::UserSiteAccess::kOnSite);
  bool is_on_all_sites_enabled = permissions_manager->CanUserSelectSiteAccess(
      *extension, url, PermissionsManager::UserSiteAccess::kOnAllSites);

  site_permissions_page->Update(extension_name, extension_icon, current_site,
                                user_site_access, is_show_requests_toggle_on,
                                is_on_site_enabled, is_on_all_sites_enabled);
}

void ExtensionsMenuViewController::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  DCHECK(current_page_);

  // Do nothing when site permission page is opened as a new extension doesn't
  // affect the site permissions page of another extension.
  if (GetSitePermissionsPage(current_page_.view())) {
    return;
  }

  // Insert a menu item for the extension when main page is opened.
  auto* main_page = GetMainPage(current_page_.view());
  DCHECK(main_page);
  int index = FindIndex(*toolbar_model_, action_id);
  InsertMenuItemMainPage(main_page, action_id, index);
}

void ExtensionsMenuViewController::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  DCHECK(current_page_);

  auto* site_permissions_page = GetSitePermissionsPage(current_page_.view());
  if (site_permissions_page) {
    // Return to the main page if site permissions page belongs to the extension
    // removed.
    if (site_permissions_page->extension_id() == action_id) {
      OpenMainPage();
    }
    return;
  }

  // Remove the menu item for the extension when main page is opened.
  auto* main_page = GetMainPage(current_page_.view());
  DCHECK(main_page);
  main_page->RemoveMenuItem(action_id);
}

void ExtensionsMenuViewController::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  UpdatePage(GetActiveWebContents());
}

void ExtensionsMenuViewController::OnToolbarModelInitialized() {
  DCHECK(current_page_);

  // Toolbar model should have been initialized if site permissions page is
  // open, since this page can only be reached after main page was populated
  // after toolbar model was initialized.
  CHECK(!GetSitePermissionsPage(current_page_.view()));

  auto* main_page = GetMainPage(current_page_.view());
  DCHECK(main_page);
  PopulateMainPage(main_page);
}

void ExtensionsMenuViewController::OnToolbarPinnedActionsChanged() {
  DCHECK(current_page_);

  // Do nothing when site permissions page is opened as it doesn't have pin
  // buttons.
  if (GetSitePermissionsPage(current_page_.view())) {
    return;
  }

  auto* main_page = GetMainPage(current_page_.view());
  DCHECK(main_page);

  std::vector<ExtensionMenuItemView*> menu_items = main_page->GetMenuItems();
  for (auto* menu_item : menu_items) {
    bool is_action_pinned =
        toolbar_model_->IsActionPinned(menu_item->view_controller()->GetId());
    menu_item->UpdateContextMenuButton(is_action_pinned);
  }
}

void ExtensionsMenuViewController::OnUserPermissionsSettingsChanged(
    const PermissionsManager::UserPermissionsSettings& settings) {
  DCHECK(current_page_);

  if (GetSitePermissionsPage(current_page_.view())) {
    // Site permissions page can only be opened when site setting is set to
    // "customize by extension". Thus, when site settings changed, we have to
    // return to main page.
    DCHECK_NE(PermissionsManager::Get(browser_->profile())
                  ->GetUserSiteSetting(GetActiveWebContents()
                                           ->GetPrimaryMainFrame()
                                           ->GetLastCommittedOrigin()),
              PermissionsManager::UserSiteSetting::kCustomizeByExtension);
    OpenMainPage();
    return;
  }

  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  DCHECK(main_page);
  UpdateMainPage(main_page, GetActiveWebContents());

  // TODO(crbug.com/40879945): Update the "highlighted section" based on the
  // `site_setting` and whether a page refresh is needed.

  // TODO(crbug.com/40879945): Run blocked actions for extensions that only have
  // blocked actions that don't require a page refresh to run.
}

void ExtensionsMenuViewController::OnShowAccessRequestsInToolbarChanged(
    const extensions::ExtensionId& extension_id,
    bool can_show_requests) {
  DCHECK(current_page_);

  // Changing whether an extension can show requests access in the toolbar only
  // affects the site permissions page for such extension.
  auto* site_permissions_page = GetSitePermissionsPage(current_page_.view());
  if (site_permissions_page &&
      site_permissions_page->extension_id() == extension_id) {
    site_permissions_page->UpdateShowRequestsToggle(can_show_requests);
  }
}

void ExtensionsMenuViewController::OnSiteAccessRequestDismissedByUser(
    const extensions::ExtensionId& extension_id,
    const url::Origin& origin) {
  DCHECK(current_page_);

  // Extension can only dismiss requests from the menu's main page. if it has
  // navigated to another site in between, do nothing (navigation listeners will
  // handle menu updates).
  auto* main_page = GetMainPage(current_page_.view());
  if (!main_page ||
      GetActiveWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin() !=
          origin) {
    return;
  }

  main_page->RemoveExtensionRequestingAccess(extension_id);
}

void ExtensionsMenuViewController::OnSiteAccessRequestAdded(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  DCHECK(current_page_);

  // Ignore requests for other tabs.
  int current_tab_id =
      extensions::ExtensionTabUtil::GetTabId(GetActiveWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  // Site access requests only affect the 'user customized access' section in
  // the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page || main_page->GetMessageSectionState() !=
                        ExtensionsMenuMainPageView::MessageSectionState::
                            kUserCustomizedAccess) {
    return;
  }

  // Add the request iff it's an active one.
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser_->profile());
  if (permissions_manager->HasActiveSiteAccessRequest(tab_id, extension_id)) {
    // TODO(crbug.com/330588494): Add to correct index based on alphabetic
    // order.
    int index = 0;
    AddOrUpdateExtensionRequestingAccess(main_page, extension_id, index,
                                         GetActiveWebContents());
  }
}

void ExtensionsMenuViewController::OnSiteAccessRequestUpdated(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  DCHECK(current_page_);

  // Ignore requests for other tabs.
  int current_tab_id =
      extensions::ExtensionTabUtil::GetTabId(GetActiveWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  // Site access requests only affect the 'user customized access' section in
  // the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page || main_page->GetMessageSectionState() !=
                        ExtensionsMenuMainPageView::MessageSectionState::
                            kUserCustomizedAccess) {
    return;
  }

  // Update the request iff it's an active one.
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser_->profile());
  if (permissions_manager->HasActiveSiteAccessRequest(tab_id, extension_id)) {
    // TODO(crbug.com/330588494): Add to correct index based on alphabetic
    // order.
    int index = 0;
    AddOrUpdateExtensionRequestingAccess(main_page, extension_id, index,
                                         GetActiveWebContents());
    return;
  }

  // Otherwise, remove the request if existent.
  main_page->RemoveExtensionRequestingAccess(extension_id);
}

void ExtensionsMenuViewController::OnSiteAccessRequestRemoved(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  DCHECK(current_page_);

  // Ignore requests for other tabs.
  int current_tab_id =
      extensions::ExtensionTabUtil::GetTabId(GetActiveWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  // Site access requests only affect the 'user customized access' section in
  // the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page || main_page->GetMessageSectionState() !=
                        ExtensionsMenuMainPageView::MessageSectionState::
                            kUserCustomizedAccess) {
    return;
  }

  main_page->RemoveExtensionRequestingAccess(extension_id);
}

void ExtensionsMenuViewController::OnSiteAccessRequestsCleared(int tab_id) {
  DCHECK(current_page_);

  // Ignore requests for other tabs.
  int current_tab_id =
      extensions::ExtensionTabUtil::GetTabId(GetActiveWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  // Site access requests only affect the 'user customized access' section in
  // the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page || main_page->GetMessageSectionState() !=
                        ExtensionsMenuMainPageView::MessageSectionState::
                            kUserCustomizedAccess) {
    return;
  }

  main_page->ClearExtensionsRequestingAccess();
}

ExtensionsMenuMainPageView*
ExtensionsMenuViewController::GetMainPageViewForTesting() {
  DCHECK(current_page_);
  return GetMainPage(current_page_.view());
}

ExtensionsMenuSitePermissionsPageView*
ExtensionsMenuViewController::GetSitePermissionsPageForTesting() {
  DCHECK(current_page_);
  return GetSitePermissionsPage(current_page_.view());
}

void ExtensionsMenuViewController::SwitchToPage(
    std::unique_ptr<views::View> page) {
  if (current_page_) {
    bubble_contents_->RemoveChildViewT(current_page_.view());
  }
  DCHECK(!current_page_);
  current_page_.SetView(bubble_contents_->AddChildView(std::move(page)));
}

void ExtensionsMenuViewController::PopulateMainPage(
    ExtensionsMenuMainPageView* main_page) {
  // TODO(crbug.com/40879945): We should update the subheader here since it
  // despends in `toolbar_model_`.
  std::vector<std::string> sorted_ids = SortExtensionsByName(*toolbar_model_);
  for (size_t i = 0; i < sorted_ids.size(); ++i) {
    InsertMenuItemMainPage(main_page, sorted_ids[i], i);
  }
}

void ExtensionsMenuViewController::InsertMenuItemMainPage(
    ExtensionsMenuMainPageView* main_page,
    const extensions::ExtensionId& extension_id,
    int index) {
  // TODO(emiliapaz): Under MVC architecture, view should not own the view
  // controller. However, the current extensions structure depends on this
  // thus a major restructure is needed.
  std::unique_ptr<ExtensionActionViewController> action_controller =
      ExtensionActionViewController::Create(extension_id, browser_,
                                            extensions_container_);
  const extensions::Extension* extension = action_controller->extension();
  Profile* profile = browser_->profile();
  content::WebContents* web_contents = GetActiveWebContents();

  bool is_enterprise = HasEnterpriseForcedAccess(*extension, *profile);
  ExtensionMenuItemView::SiteAccessToggleState site_access_toggle_state =
      GetSiteAccessToggleState(*extension, *profile, *toolbar_model_,
                               *web_contents);
  ExtensionMenuItemView::SitePermissionsButtonState
      site_permissions_button_state = GetSitePermissionsButtonState(
          *extension, *profile, *toolbar_model_, *web_contents);
  ExtensionMenuItemView::SitePermissionsButtonAccess
      site_permissions_button_access = GetSitePermissionsButtonAccess(
          *extension, *profile, *toolbar_model_, *web_contents);

  main_page->CreateAndInsertMenuItem(std::move(action_controller), extension_id,
                                     is_enterprise, site_access_toggle_state,
                                     site_permissions_button_state,
                                     site_permissions_button_access, index);
}

void ExtensionsMenuViewController::AddOrUpdateExtensionRequestingAccess(
    ExtensionsMenuMainPageView* main_page,
    const extensions::ExtensionId& extension_id,
    int index,
    content::WebContents* web_contents) {
  ToolbarActionViewController* action_controller =
      extensions_container_->GetActionForId(extension_id);
  std::u16string name = action_controller->GetActionName();
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  ui::ImageModel icon =
      action_controller->GetIcon(web_contents, gfx::Size(icon_size, icon_size));

  main_page->AddOrUpdateExtensionRequestingAccess(extension_id, name, icon,
                                                  index);
}

content::WebContents* ExtensionsMenuViewController::GetActiveWebContents()
    const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
