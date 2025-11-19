// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

#include "base/i18n/case_conversion.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_platform_delegate_views.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/common/permissions/permissions_data.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

// Returns the extension corresponding to `extension_id` on `profile`.
const extensions::Extension* GetExtension(
    Profile& profile,
    const extensions::ExtensionId& extension_id) {
  return extensions::ExtensionRegistry::Get(&profile)
      ->enabled_extensions()
      .GetByID(extension_id);
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

  if (extensions::ExtensionSystem::Get(&profile)
          ->management_policy()
          ->HasEnterpriseForcedAccess(extension)) {
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
          extensions::ExtensionSystem::Get(&profile)
              ->management_policy()
              ->HasEnterpriseForcedAccess(extension);
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

// Returns the state for the `extension`'s site permissions button.
ExtensionsMenuViewModel::MenuItemInfo::SitePermissionsButtonState
GetSitePermissionsButtonState(const extensions::Extension& extension,
                              Profile& profile,
                              const ToolbarActionsModel& toolbar_model,
                              content::WebContents& web_contents) {
  bool is_site_permissions_button_visible = IsSitePermissionsButtonVisible(
      extension, profile, toolbar_model, web_contents);
  if (!is_site_permissions_button_visible) {
    return ExtensionsMenuViewModel::MenuItemInfo::SitePermissionsButtonState::
        kHidden;
  }

  bool is_site_permissions_button_enabled = CanUserCustomizeExtensionSiteAccess(
      extension, profile, toolbar_model, web_contents);
  return is_site_permissions_button_enabled
             ? ExtensionsMenuViewModel::MenuItemInfo::
                   SitePermissionsButtonState::kEnabled
             : ExtensionsMenuViewModel::MenuItemInfo::
                   SitePermissionsButtonState::kDisabled;
}

// Returns the sites access displayed by the `extension`'s site permissions
// button.
ExtensionsMenuViewModel::MenuItemInfo::SitePermissionsButtonAccess
GetSitePermissionsButtonAccess(const extensions::Extension& extension,
                               Profile& profile,
                               const ToolbarActionsModel& toolbar_model,
                               content::WebContents& web_contents) {
  auto site_interaction = SitePermissionsHelper(&profile).GetSiteInteraction(
      extension, &web_contents);
  if (site_interaction == SitePermissionsHelper::SiteInteraction::kNone) {
    return ExtensionsMenuViewModel::MenuItemInfo::SitePermissionsButtonAccess::
        kNone;
  }

  auto site_access = PermissionsManager::Get(&profile)->GetUserSiteAccess(
      extension, web_contents.GetLastCommittedURL());
  switch (site_access) {
    case PermissionsManager::UserSiteAccess::kOnClick:
      return ExtensionsMenuViewModel::MenuItemInfo::
          SitePermissionsButtonAccess::kOnClick;
    case PermissionsManager::UserSiteAccess::kOnSite:
      return ExtensionsMenuViewModel::MenuItemInfo::
          SitePermissionsButtonAccess::kOnSite;
    case PermissionsManager::UserSiteAccess::kOnAllSites:
      return ExtensionsMenuViewModel::MenuItemInfo::
          SitePermissionsButtonAccess::kOnAllSites;
  }
}

// Returns the state for the `extension`'s site access toggle button.
ExtensionsMenuViewModel::MenuItemInfo::SiteAccessToggleState
GetSiteAccessToggleState(const extensions::Extension& extension,
                         Profile& profile,
                         const ToolbarActionsModel& toolbar_model,
                         content::WebContents& web_contents) {
  if (!CanUserCustomizeExtensionSiteAccess(extension, profile, toolbar_model,
                                           web_contents)) {
    return ExtensionsMenuViewModel::MenuItemInfo::SiteAccessToggleState::
        kHidden;
  }

  // Button is on iff the extension has access to the site.
  auto site_interaction = SitePermissionsHelper(&profile).GetSiteInteraction(
      extension, &web_contents);
  return site_interaction == SitePermissionsHelper::SiteInteraction::kGranted
             ? ExtensionsMenuViewModel::MenuItemInfo::SiteAccessToggleState::kOn
             : ExtensionsMenuViewModel::MenuItemInfo::SiteAccessToggleState::
                   kOff;
}

void LogShowHostAccessRequestInToolbar(bool show) {
  if (show) {
    base::RecordAction(base::UserMetricsAction(
        "Extensions.Menu.ShowRequestsInToolbarPressed"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Extensions.Menu.HideRequestsInToolbarPressed"));
  }
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
      NOTREACHED() << "Unknown site access";
  }
}

void LogSiteSettingsUpdate(PermissionsManager::UserSiteSetting site_setting) {
  switch (site_setting) {
    case PermissionsManager::UserSiteSetting::kCustomizeByExtension:
      base::RecordAction(
          base::UserMetricsAction("Extensions.Menu.AllowByExtensionSelected"));
      break;
    case PermissionsManager::UserSiteSetting::kBlockAllExtensions:
      base::RecordAction(
          base::UserMetricsAction("Extensions.Menu.ExtensionsBlockedSelected"));
      break;
    case PermissionsManager::UserSiteSetting::kGrantAllExtensions:
    default:
      NOTREACHED() << "Invalid site setting update";
  }
}

base::debug::CrashKeyString* GetCurrentSiteAccessCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_site_access_before_granting", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetCurrentSiteInteractionCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_site_interaction_before_granting",
      base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetCurrentUrlCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_web_contents_before_granting", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetSiteAccessToggleStateCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_toggle_state_before_granting", base::debug::CrashKeySize::Size64);
  return crash_key;
}

std::string GetCurrentSiteAccessCrashValue(
    PermissionsManager::UserSiteAccess site_access) {
  switch (site_access) {
    case PermissionsManager::UserSiteAccess::kOnClick:
      return "OnClick";
    case PermissionsManager::UserSiteAccess::kOnSite:
      return "OnSite";
    case PermissionsManager::UserSiteAccess::kOnAllSites:
      return "OnAllSites";
    default:
      return "InvalidValue";
  }
}

std::string GetCurrentSiteInteractionCrashValue(
    SitePermissionsHelper::SiteInteraction site_interaction) {
  switch (site_interaction) {
    case SitePermissionsHelper::SiteInteraction::kNone:
      return "None";
    case SitePermissionsHelper::SiteInteraction::kWithheld:
      return "Withheld";
    case SitePermissionsHelper::SiteInteraction::kActiveTab:
      return "ActiveTab";
    case SitePermissionsHelper::SiteInteraction::kGranted:
      return "Granted";
    default:
      return "InvalidValue";
  }
}

std::string GetCurrentUrlCrashValue(content::WebContents* web_contents) {
  if (!web_contents) {
    return "empty web conents";
  }
  auto url = web_contents->GetLastCommittedURL();
  if (url.is_empty()) {
    return "empty url";
  }
  return url.GetHost();
}

std::string GetSiteAccessToggleStateCrashValue(
    ExtensionsMenuViewModel::MenuItemInfo::SiteAccessToggleState state) {
  switch (state) {
    case ExtensionsMenuViewModel::MenuItemInfo::SiteAccessToggleState::kOn:
      return "On";
    case ExtensionsMenuViewModel::MenuItemInfo::SiteAccessToggleState::kOff:
      return "Off";
    case ExtensionsMenuViewModel::MenuItemInfo::SiteAccessToggleState::kHidden:
      return "Hidden";
    default:
      return "InvalidValue";
  }
}

}  // namespace

namespace debug {

// Helper for adding a crash keys when we encounter crashes when granting site
// access.
//
// All keys are logged every time this class is instantiated.
// TODO(crbug.com/456129773): Remove when crash is fixed.
class ScopedGrantSiteAccessCrashKeys {
 public:
  explicit ScopedGrantSiteAccessCrashKeys(
      PermissionsManager::UserSiteAccess current_site_access,
      SitePermissionsHelper::SiteInteraction current_site_interaction,
      content::WebContents* web_contents,
      ExtensionsMenuViewModel::MenuItemInfo::SiteAccessToggleState
          site_access_toggle_state)
      : current_site_access_crash_key_(
            GetCurrentSiteAccessCrashKey(),
            GetCurrentSiteAccessCrashValue(current_site_access)),
        current_site_interaction_crash_key_(
            GetCurrentSiteInteractionCrashKey(),
            GetCurrentSiteInteractionCrashValue(current_site_interaction)),
        current_url_crash_key_(GetCurrentUrlCrashKey(),
                               GetCurrentUrlCrashValue(web_contents)),
        site_access_toggle_state_crash_key_(
            GetSiteAccessToggleStateCrashKey(),
            GetSiteAccessToggleStateCrashValue(site_access_toggle_state)) {}
  ~ScopedGrantSiteAccessCrashKeys() = default;

 private:
  // The current site access of the extension when GrantSiteAccess() was called.
  base::debug::ScopedCrashKeyString current_site_access_crash_key_;
  // The current site interaction of the extension when GrantSiteAccess() was
  // called.
  base::debug::ScopedCrashKeyString current_site_interaction_crash_key_;
  // The current URL (hostname) when GrantSiteAccess() was called.
  base::debug::ScopedCrashKeyString current_url_crash_key_;
  // The site access toggle state of the extension before toggle was selected
  // that calls GrantSiteAccess().
  base::debug::ScopedCrashKeyString site_access_toggle_state_crash_key_;
};

}  // namespace debug

ExtensionsMenuViewModel::ExtensionsMenuViewModel(
    BrowserWindowInterface* browser,
    std::unique_ptr<ExtensionsMenuViewPlatformDelegate> platform_delegate)
    : browser_(browser),
      platform_delegate_(std::move(platform_delegate)),
      toolbar_model_(ToolbarActionsModel::Get(browser_->GetProfile())) {
  platform_delegate_->AttachToModel(this);

  permissions_manager_observation_.Observe(
      extensions::PermissionsManager::Get(browser_->GetProfile()));
  toolbar_model_observation_.Observe(toolbar_model_.get());
  auto* tab_list = TabListInterface::From(browser);
  tab_list_interface_observation_.Observe(tab_list);
}

ExtensionsMenuViewModel::~ExtensionsMenuViewModel() {
  platform_delegate_->DetachFromModel();
}

void ExtensionsMenuViewModel::UpdateSiteAccess(
    const extensions::ExtensionId& extension_id,
    PermissionsManager::UserSiteAccess site_access) {
  LogSiteAccessUpdate(site_access);

  Profile* profile = browser_->GetProfile();
  SitePermissionsHelper permissions(profile);
  permissions.UpdateSiteAccess(*GetExtension(*profile, extension_id),
                               GetActiveWebContents(), site_access);
}

void ExtensionsMenuViewModel::AllowHostAccessRequest(
    const extensions::ExtensionId& extension_id) {
  content::WebContents* web_contents = GetActiveWebContents();
  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  if (!action_runner) {
    return;
  }

  // Accepting a host access request grants always access to the site.
  Profile* profile = browser_->GetProfile();
  extensions::SitePermissionsHelper(profile).UpdateSiteAccess(
      *GetExtension(*profile, extension_id), web_contents,
      extensions::PermissionsManager::UserSiteAccess::kOnSite);

  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.ExtensionActivatedFromAllowingRequestAccessInMenu"));
}

void ExtensionsMenuViewModel::DismissHostAccessRequest(
    const extensions::ExtensionId& extension_id) {
  auto* permissions_manager = PermissionsManager::Get(browser_->GetProfile());
  CHECK(permissions_manager);
  content::WebContents* web_contents = GetActiveWebContents();
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  permissions_manager->UserDismissedHostAccessRequest(web_contents, tab_id,
                                                      extension_id);

  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.ExtensionRequestDismissedFromMenu"));
}

void ExtensionsMenuViewModel::ShowHostAccessRequestsInToolbar(
    const extensions::ExtensionId& extension_id,
    bool show) {
  extensions::SitePermissionsHelper(browser_->GetProfile())
      .SetShowAccessRequestsInToolbar(extension_id, show);

  LogShowHostAccessRequestInToolbar(show);
}

void ExtensionsMenuViewModel::GrantSiteAccess(
    const extensions::ExtensionId& extension_id) {
  auto* profile = browser_->GetProfile();
  const extensions::Extension* extension = GetExtension(*profile, extension_id);
  content::WebContents* web_contents = GetActiveWebContents();
  auto* toolbar_model = ToolbarActionsModel::Get(profile);
  auto url = web_contents->GetLastCommittedURL();
  auto* permissions_manager = PermissionsManager::Get(profile);

  // Can only grant site access when user can customize the extension's site
  // access and it's currently on click.
  auto current_site_access =
      permissions_manager->GetUserSiteAccess(*extension, url);
  CHECK(CanUserCustomizeExtensionSiteAccess(*extension, *profile,
                                            *toolbar_model, *web_contents));

  // TODO(crbug.com/456129773): Remove crash keys when site toggle crash is
  // fixed.
  auto current_site_interaction =
      SitePermissionsHelper(profile).GetSiteInteraction(*extension,
                                                        web_contents);
  debug::ScopedGrantSiteAccessCrashKeys grant_site_access_crash_keys(
      current_site_access, current_site_interaction, web_contents,
      GetSiteAccessToggleState(*extension, *profile, *toolbar_model,
                               *web_contents));
  DCHECK_EQ(current_site_access, PermissionsManager::UserSiteAccess::kOnClick);

  // Update site access when extension requested host permissions for the
  // current site (that is, site access was withheld).
  PermissionsManager::ExtensionSiteAccess extension_site_access =
      permissions_manager->GetSiteAccess(*extension, url);
  if (extension_site_access.withheld_site_access ||
      extension_site_access.withheld_all_sites_access) {
    // Restore to previous access by looking whether broad site access was
    // previously granted.
    PermissionsManager::UserSiteAccess new_site_access =
        permissions_manager->HasPreviousBroadSiteAccess(extension_id)
            ? PermissionsManager::UserSiteAccess::kOnAllSites
            : PermissionsManager::UserSiteAccess::kOnSite;
    SitePermissionsHelper permissions_helper(profile);
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
}

void ExtensionsMenuViewModel::RevokeSiteAccess(
    const extensions::ExtensionId& extension_id) {
  auto* profile = browser_->GetProfile();
  const extensions::Extension* extension = GetExtension(*profile, extension_id);
  content::WebContents* web_contents = GetActiveWebContents();
  auto* toolbar_model = ToolbarActionsModel::Get(profile);

  // Can only revoke site access when user can customize the extension's site
  // access.
  CHECK(CanUserCustomizeExtensionSiteAccess(*extension, *profile,
                                            *toolbar_model, *web_contents));

  auto url = web_contents->GetLastCommittedURL();
  auto* permissions_manager = PermissionsManager::Get(profile);
  auto current_site_access =
      permissions_manager->GetUserSiteAccess(*extension, url);
  PermissionsManager::ExtensionSiteAccess extension_site_access =
      permissions_manager->GetSiteAccess(*extension, url);

  // Update site access to "on click" when extension requested, and was granted,
  // host permissions for the current site (that is, extension has site access).
  if (extension_site_access.has_site_access ||
      extension_site_access.has_all_sites_access) {
    CHECK_NE(current_site_access, PermissionsManager::UserSiteAccess::kOnClick);
    SitePermissionsHelper permissions_helper(profile);
    permissions_helper.UpdateSiteAccess(
        *extension, web_contents, PermissionsManager::UserSiteAccess::kOnClick);
    return;
  }

  // Otherwise, extension has one-time access and we need to clear tab
  // permissions (e.g extension with activeTab was granted one-time access).
  CHECK_EQ(current_site_access, PermissionsManager::UserSiteAccess::kOnClick);
  extensions::ActiveTabPermissionGranter::FromWebContents(web_contents)
      ->ClearActiveExtensionAndNotify(extension_id);

  auto* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  if (action_runner) {
    action_runner->ShowReloadPageBubble({GetExtension(*profile, extension_id)});
  }
}

void ExtensionsMenuViewModel::UpdateSiteSetting(
    extensions::PermissionsManager::UserSiteSetting site_setting) {
  content::WebContents* web_contents = GetActiveWebContents();
  const url::Origin& origin =
      GetActiveWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  extensions::TabHelper::FromWebContents(web_contents)
      ->SetReloadRequired(site_setting);
  PermissionsManager::Get(browser_->GetProfile())
      ->UpdateUserSiteSetting(origin, site_setting);

  LogSiteSettingsUpdate(site_setting);
}

void ExtensionsMenuViewModel::ReloadWebContents() {
  GetActiveWebContents()->GetController().Reload(content::ReloadType::NORMAL,
                                                 false);
}

ExtensionsMenuViewModel::MenuItemInfo ExtensionsMenuViewModel::GetMenuItemInfo(
    ToolbarActionViewModel* action_model) {
  Profile* profile = browser_->GetProfile();
  // TODO(crbug.com/456285449): If there is an action controller, then the
  // extension should be enabled. We should retrieve the extension from the
  // extension action controller, but here we are getting the toolbar action
  // controller interface. For that we either need to (a) pass the action view
  // controller instead or (b) add the extension getter method to toolbar action
  // view controller.
  auto* registry = extensions::ExtensionRegistry::Get(profile);
  scoped_refptr<const extensions::Extension> extension =
      registry->enabled_extensions().GetByID(action_model->GetId());
  CHECK(extension);
  content::WebContents* web_contents = GetActiveWebContents();

  MenuItemInfo menu_item;
  menu_item.site_access_toggle_state = GetSiteAccessToggleState(
      *extension, *profile, *toolbar_model_, *web_contents);
  menu_item.site_permissions_button_access = GetSitePermissionsButtonAccess(
      *extension, *profile, *toolbar_model_, *web_contents);
  menu_item.site_permissions_button_state = GetSitePermissionsButtonState(
      *extension, *profile, *toolbar_model_, *web_contents);
  menu_item.is_enterprise = extensions::ExtensionSystem::Get(profile)
                                ->management_policy()
                                ->HasEnterpriseForcedAccess(*extension);
  return menu_item;
}

void ExtensionsMenuViewModel::OnHostAccessRequestAdded(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  // Ignore requests for other tabs.
  auto* web_contents = GetActiveWebContents();
  int current_tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id != current_tab_id) {
    return;
  }

  // Ignore requests that are not active.
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser_->GetProfile());
  if (!permissions_manager->HasActiveHostAccessRequest(tab_id, extension_id)) {
    return;
  }

  platform_delegate_->OnHostAccessRequestAddedOrUpdated(extension_id,
                                                        web_contents);
}

void ExtensionsMenuViewModel::OnHostAccessRequestUpdated(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  // Ignore requests for other tabs.
  int current_tab_id =
      extensions::ExtensionTabUtil::GetTabId(GetActiveWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser_->GetProfile());
  if (permissions_manager->HasActiveHostAccessRequest(tab_id, extension_id)) {
    // Update the request iff it's an active one.
    platform_delegate_->OnHostAccessRequestAddedOrUpdated(
        extension_id, GetActiveWebContents());
  } else {
    // Otherwise, remove the request if existent.
    platform_delegate_->OnHostAccessRequestRemoved(extension_id);
  }
}

void ExtensionsMenuViewModel::OnHostAccessRequestRemoved(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  // Ignore requests for other tabs.
  auto* web_contents = GetActiveWebContents();
  int current_tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id != current_tab_id) {
    return;
  }

  platform_delegate_->OnHostAccessRequestRemoved(extension_id);
}

void ExtensionsMenuViewModel::OnHostAccessRequestsCleared(int tab_id) {
  // Ignore requests for other tabs.
  int current_tab_id =
      extensions::ExtensionTabUtil::GetTabId(GetActiveWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  platform_delegate_->OnHostAccessRequestsCleared();
}

void ExtensionsMenuViewModel::OnHostAccessRequestDismissedByUser(
    const extensions::ExtensionId& extension_id,
    const url::Origin& origin) {
  // Ignore request dismissal if web contents have navigated to a different
  // origin from where the request originated, as navigation listeners will
  // handle menu updates.
  if (GetActiveWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin() !=
      origin) {
    return;
  }

  platform_delegate_->OnHostAccessRequestDismissedByUser(extension_id);
}

void ExtensionsMenuViewModel::OnShowAccessRequestsInToolbarChanged(
    const extensions::ExtensionId& extension_id,
    bool can_show_requests) {
  platform_delegate_->OnShowHostAccessRequestsInToolbarChanged(
      extension_id, can_show_requests);
}

void ExtensionsMenuViewModel::OnUserPermissionsSettingsChanged(
    const extensions::PermissionsManager::UserPermissionsSettings& settings) {
  platform_delegate_->OnPermissionsSettingsChanged();
}

void ExtensionsMenuViewModel::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  platform_delegate_->OnToolbarActionAdded(action_id);
}

void ExtensionsMenuViewModel::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  platform_delegate_->OnToolbarActionRemoved(action_id);
}

void ExtensionsMenuViewModel::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  platform_delegate_->OnToolbarActionUpdated();
}

void ExtensionsMenuViewModel::OnToolbarModelInitialized() {
  platform_delegate_->OnToolbarModelInitialized();
}

void ExtensionsMenuViewModel::OnToolbarPinnedActionsChanged() {
  platform_delegate_->OnToolbarPinnedActionsChanged();
}

void ExtensionsMenuViewModel::OnActiveTabChanged(tabs::TabInterface* tab) {
  auto* web_contents = tab->GetContents();
  platform_delegate_->OnActiveWebContentsChanged(web_contents);
}

void ExtensionsMenuViewModel::DidFinishNavigation(
    content::NavigationHandle* handle) {
  auto* web_contents = GetActiveWebContents();
  platform_delegate_->OnActiveWebContentsChanged(web_contents);
}

content::WebContents* ExtensionsMenuViewModel::GetActiveWebContents() {
  return TabListInterface::From(browser_)->GetActiveTab()->GetContents();
}
