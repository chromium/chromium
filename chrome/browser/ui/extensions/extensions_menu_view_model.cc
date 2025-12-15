// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

#include <string>

#include "base/i18n/case_conversion.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_platform_delegate_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

// The state of the main page in the menu, corresponding to the current site's
// access restrictions.
enum class MainPageState {
  // Site is restricted to all extensions.
  kRestrictedSite,
  // Site is restricted all non-enterprise extensions by policy.
  kPolicyBlockedSite,
  // User blocked all extensions access to the site.
  kUserBlockedSite,
  // User can customize each extension's access to the site.
  kUserCustomizedSite,
};

// Returns the state for the main page based on the current `web_contents` URL
// and the profile's user settings.
MainPageState GetMainPageState(Profile& profile,
                               const ToolbarActionsModel& toolbar_model,
                               content::WebContents& web_contents) {
  const GURL& url = web_contents.GetLastCommittedURL();
  if (toolbar_model.IsRestrictedUrl(url)) {
    return MainPageState::kRestrictedSite;
  }

  if (toolbar_model.IsPolicyBlockedHost(url)) {
    return MainPageState::kPolicyBlockedSite;
  }

  PermissionsManager::UserSiteSetting site_setting =
      PermissionsManager::Get(&profile)->GetUserSiteSetting(
          web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin());
  if (site_setting ==
      PermissionsManager::UserSiteSetting::kBlockAllExtensions) {
    return MainPageState::kUserBlockedSite;
  }

  return MainPageState::kUserCustomizedSite;
}

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

// Returns the status for the site permissions button.
ExtensionsMenuViewModel::ControlState::Status GetSitePermissionsButtonStatus(
    const extensions::Extension& extension,
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents,
    bool is_enterprise,
    SitePermissionsHelper::SiteInteraction site_interaction) {
  auto url = web_contents.GetLastCommittedURL();

  // Button is hidden when site is restricted.
  if (toolbar_model.IsRestrictedUrl(url)) {
    return ExtensionsMenuViewModel::ControlState::Status::kHidden;
  }

  // Button is disabled when site is blocked by policy.
  // TODO(crbug.com/40879945): Consider only showing the site permissions
  // button only for enterprise installed extensions on policy-blocked
  // sites, similar to how we do for user-blocked sites.
  if (extension.permissions_data()->IsPolicyBlockedHost(url)) {
    return ExtensionsMenuViewModel::ControlState::Status::kDisabled;
  }

  auto user_site_setting =
      PermissionsManager::Get(&profile)->GetUserSiteSetting(
          web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin());

  if (is_enterprise) {
    // Button is hidden when enterprise extension has no granted access on a
    // user-blocked site.
    if (user_site_setting ==
            PermissionsManager::UserSiteSetting::kBlockAllExtensions &&
        site_interaction == SitePermissionsHelper::SiteInteraction::kNone) {
      return ExtensionsMenuViewModel::ControlState::Status::kHidden;
    }
    // Otherwise, button is disabled.
    return ExtensionsMenuViewModel::ControlState::Status::kDisabled;
  }

  // Button is hidden for non-enterprise extension when user blocked extensions
  // on the site.
  if (user_site_setting ==
      PermissionsManager::UserSiteSetting::kBlockAllExtensions) {
    return ExtensionsMenuViewModel::ControlState::Status::kHidden;
  }

  // Button is enabled for non-enterprise extension when user can customize the
  // extension's site access.
  if (CanUserCustomizeExtensionSiteAccess(extension, profile, toolbar_model,
                                          web_contents)) {
    return ExtensionsMenuViewModel::ControlState::Status::kEnabled;
  }

  // Otherwise, button is disabled for non-enterprise extensions.
  return ExtensionsMenuViewModel::ControlState::Status::kDisabled;
}

std::u16string GetSitePermissionsButtonText(
    const extensions::Extension& extension,
    Profile& profile,
    content::WebContents& web_contents,
    SitePermissionsHelper::SiteInteraction site_interaction) {
  if (site_interaction == SitePermissionsHelper::SiteInteraction::kNone) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_NONE);
  }

  int label_id;
  auto site_access = PermissionsManager::Get(&profile)->GetUserSiteAccess(
      extension, web_contents.GetLastCommittedURL());
  switch (site_access) {
    case PermissionsManager::UserSiteAccess::kOnClick:
      label_id = IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK;
      break;
    case PermissionsManager::UserSiteAccess::kOnSite:
      label_id = IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_SITE;
      break;
    case PermissionsManager::UserSiteAccess::kOnAllSites:
      label_id =
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES;
      break;
  }
  return l10n_util::GetStringUTF16(label_id);
}

std::u16string GetSitePermissionsButtonTooltip(
    bool is_enterprise,
    SitePermissionsHelper::SiteInteraction site_interaction) {
  if (is_enterprise) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_MENU_MAIN_PAGE_ENTERPRISE_EXTENSION_SITE_ACCESS_TOOLTIP);
  }

  if (site_interaction == SitePermissionsHelper::SiteInteraction::kNone) {
    // No tooltip is shown.
    return std::u16string();
  }

  return l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_TOOLTIP);
}

std::u16string GetSitePermissionsButtonAccName(
    bool is_enterprise,
    SitePermissionsHelper::SiteInteraction site_interaction,
    std::u16string& button_text) {
  if (is_enterprise) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_MENU_MAIN_PAGE_ENTERPRISE_EXTENSION_SITE_ACCESS_ACCESSIBLE_NAME,
        button_text);
  }

  if (site_interaction == SitePermissionsHelper::SiteInteraction::kNone) {
    return button_text;
  }

  return l10n_util::GetStringFUTF16(
      IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ACCESSIBLE_NAME,
      button_text);
}

std::u16string GetSiteAccessToggleTooltip(bool is_on) {
  return l10n_util::GetStringUTF16(
      is_on ? IDS_EXTENSIONS_MENU_EXTENSION_SITE_ACCESS_TOGGLE_ON_TOOLTIP
            : IDS_EXTENSIONS_MENU_EXTENSION_SITE_ACCESS_TOGGLE_OFF_TOOLTIP);
}

// Returns the state for the `extension`'s site permissions button.
ExtensionsMenuViewModel::ControlState GetSitePermissionsButtonState(
    const extensions::Extension& extension,
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents) {
  bool is_enterprise = extensions::ExtensionSystem::Get(&profile)
                           ->management_policy()
                           ->HasEnterpriseForcedAccess(extension);
  auto site_interaction = SitePermissionsHelper(&profile).GetSiteInteraction(
      extension, &web_contents);

  ExtensionsMenuViewModel::ControlState site_permissions_button_state;
  site_permissions_button_state.status = GetSitePermissionsButtonStatus(
      extension, profile, toolbar_model, web_contents, is_enterprise,
      site_interaction);
  site_permissions_button_state.text = GetSitePermissionsButtonText(
      extension, profile, web_contents, site_interaction);
  site_permissions_button_state.accessible_name =
      GetSitePermissionsButtonAccName(is_enterprise, site_interaction,
                                      site_permissions_button_state.text);
  site_permissions_button_state.tooltip_text =
      GetSitePermissionsButtonTooltip(is_enterprise, site_interaction);

  return site_permissions_button_state;
}

// Returns the state for the `extension`'s site access toggle button.
ExtensionsMenuViewModel::ControlState GetSiteAccessToggleState(
    const extensions::Extension& extension,
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents) {
  ExtensionsMenuViewModel::ControlState toggle_state;
  toggle_state.status =
      CanUserCustomizeExtensionSiteAccess(extension, profile, toolbar_model,
                                          web_contents)
          ? ExtensionsMenuViewModel::ControlState::Status::kEnabled
          : ExtensionsMenuViewModel::ControlState::Status::kHidden;

  // Button is on iff the extension has access to the site.
  auto site_interaction = SitePermissionsHelper(&profile).GetSiteInteraction(
      extension, &web_contents);
  toggle_state.is_on =
      site_interaction == SitePermissionsHelper::SiteInteraction::kGranted;

  toggle_state.tooltip_text = GetSiteAccessToggleTooltip(toggle_state.is_on);

  return toggle_state;
}

// Returns the radio button text for `site_access` option.
std::u16string GetSiteAccessOptionText(
    PermissionsManager::UserSiteAccess site_access,
    std::u16string current_site = std::u16string()) {
  switch (site_access) {
    case PermissionsManager::UserSiteAccess::kOnClick:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SITE_ACCESS_ON_CLICK_TEXT);
    case PermissionsManager::UserSiteAccess::kOnSite:
      return l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SITE_ACCESS_ON_SITE_TEXT,
          current_site);
    case PermissionsManager::UserSiteAccess::kOnAllSites:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SITE_ACCESS_ON_ALL_SITES_TEXT);
    default:
      NOTREACHED();
  }
}

// Returns the accessible name for the shows requests toggle given whether it is
// on.
std::u16string GetShowRequestsToggleAccessibleName(bool is_toggle_on) {
  int label_id =
      is_toggle_on
          ? IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SHOW_REQUESTS_TOGGLE_ON
          : IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SHOW_REQUESTS_TOGGLE_OFF;
  return l10n_util::GetStringUTF16(label_id);
}

// Updates the site settings toggle text based on its state.
std::u16string GetSiteSettingToggleText(bool is_on) {
  int label_id = is_on ? IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_ON_TOOLTIP
                       : IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_OFF_TOOLTIP;
  return l10n_util::GetStringUTF16(label_id);
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
      content::WebContents* web_contents)
      : current_site_access_crash_key_(
            GetCurrentSiteAccessCrashKey(),
            GetCurrentSiteAccessCrashValue(current_site_access)),
        current_site_interaction_crash_key_(
            GetCurrentSiteInteractionCrashKey(),
            GetCurrentSiteInteractionCrashValue(current_site_interaction)),
        current_url_crash_key_(GetCurrentUrlCrashKey(),
                               GetCurrentUrlCrashValue(web_contents)) {}
  ~ScopedGrantSiteAccessCrashKeys() = default;

 private:
  // The current site access of the extension when GrantSiteAccess() was called.
  base::debug::ScopedCrashKeyString current_site_access_crash_key_;
  // The current site interaction of the extension when GrantSiteAccess() was
  // called.
  base::debug::ScopedCrashKeyString current_site_interaction_crash_key_;
  // The current URL (hostname) when GrantSiteAccess() was called.
  base::debug::ScopedCrashKeyString current_url_crash_key_;
};

}  // namespace debug

ExtensionsMenuViewModel::ControlState::ControlState() = default;
ExtensionsMenuViewModel::ControlState::ControlState(const ControlState&) =
    default;
ExtensionsMenuViewModel::ControlState&
ExtensionsMenuViewModel::ControlState::operator=(const ControlState&) = default;
ExtensionsMenuViewModel::ControlState::~ControlState() = default;

ExtensionsMenuViewModel::ExtensionsMenuViewModel(
    BrowserWindowInterface* browser)
    : browser_(browser),
      toolbar_model_(ToolbarActionsModel::Get(browser_->GetProfile())) {
  permissions_manager_observation_.Observe(
      extensions::PermissionsManager::Get(browser_->GetProfile()));
  toolbar_model_observation_.Observe(toolbar_model_.get());
  auto* tab_list = TabListInterface::From(browser);
  tab_list_interface_observation_.Observe(tab_list);
}

ExtensionsMenuViewModel::~ExtensionsMenuViewModel() = default;

void ExtensionsMenuViewModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionsMenuViewModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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
      current_site_access, current_site_interaction, web_contents);
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


ExtensionsMenuViewModel::ExtensionSiteAccessOptionsState
ExtensionsMenuViewModel::GetExtensionSiteAccessOptionsState(
    const extensions::ExtensionId& extension_id) {
  Profile* profile = browser_->GetProfile();
  auto* permissions_manager = PermissionsManager::Get(profile);

  // TODO(crbug.com/456285449): If there is an action controller, then the
  // extension should be enabled. We should retrieve the extension from the
  // extension action view model, but here we are getting the toolbar action
  // view model interface. For that we either need to (a) pass the action view
  // controller instead or (b) add the extension getter method to toolbar action
  // view controller.
  scoped_refptr<const extensions::Extension> extension =
      GetExtension(*profile, extension_id);
  CHECK(extension);
  content::WebContents* web_contents = GetActiveWebContents();
  const GURL& url = web_contents->GetLastCommittedURL();

  // Extension's site permissions can only be compute when such can be modified
  // by the user.
  CHECK(CanUserCustomizeExtensionSiteAccess(*extension, *profile,
                                            *toolbar_model_, *web_contents));
  auto site_access = permissions_manager->GetUserSiteAccess(*extension, url);

  ControlState on_click_option;
  on_click_option.status = ControlState::Status::kEnabled;
  on_click_option.text =
      GetSiteAccessOptionText(PermissionsManager::UserSiteAccess::kOnClick);
  on_click_option.is_on =
      site_access == PermissionsManager::UserSiteAccess::kOnClick;

  ControlState on_site_option;
  on_site_option.status =
      permissions_manager->CanUserSelectSiteAccess(
          *extension, url, PermissionsManager::UserSiteAccess::kOnSite)
          ? ControlState::Status::kEnabled
          : ControlState::Status::kDisabled;
  on_site_option.text =
      GetSiteAccessOptionText(PermissionsManager::UserSiteAccess::kOnSite);
  on_site_option.is_on =
      site_access == PermissionsManager::UserSiteAccess::kOnSite;

  ControlState on_all_sites_option;
  on_all_sites_option.status =
      permissions_manager->CanUserSelectSiteAccess(
          *extension, url, PermissionsManager::UserSiteAccess::kOnAllSites)
          ? ControlState::Status::kEnabled
          : ControlState::Status::kDisabled;
  on_all_sites_option.text =
      GetSiteAccessOptionText(PermissionsManager::UserSiteAccess::kOnAllSites);
  on_all_sites_option.is_on =
      site_access == PermissionsManager::UserSiteAccess::kOnAllSites;

  ExtensionSiteAccessOptionsState extension_site_access;
  extension_site_access.on_click_option = on_click_option;
  extension_site_access.on_site_option = on_site_option;
  extension_site_access.on_all_sites_option = on_all_sites_option;

  return extension_site_access;
}

ExtensionsMenuViewModel::ControlState
ExtensionsMenuViewModel::GetExtensionShowRequestsToggleState(
    const extensions::ExtensionId& extension_id) {
  SitePermissionsHelper permissions_helper(browser_->GetProfile());
  bool is_toggle_on =
      permissions_helper.ShowAccessRequestsInToolbar(extension_id);

  ControlState show_requests_toggle;
  show_requests_toggle.status = ControlState::Status::kEnabled;
  show_requests_toggle.accessible_name =
      GetShowRequestsToggleAccessibleName(is_toggle_on);
  show_requests_toggle.is_on = is_toggle_on;
  return show_requests_toggle;
}

ExtensionsMenuViewModel::MenuItemState
ExtensionsMenuViewModel::GetMenuItemState(
    const extensions::ExtensionId& extension_id) {
  Profile* profile = browser_->GetProfile();
  // TODO(crbug.com/456285449): If there is an action controller, then the
  // extension should be enabled. We should retrieve the extension from the
  // extension action view model, but here we are getting the toolbar action
  // view model interface. For that we either need to (a) pass the action view
  // controller instead or (b) add the extension getter method to toolbar action
  // view controller.
  scoped_refptr<const extensions::Extension> extension =
      GetExtension(*profile, extension_id);
  CHECK(extension);
  content::WebContents* web_contents = GetActiveWebContents();

  MenuItemState menu_item;
  menu_item.site_access_toggle = GetSiteAccessToggleState(
      *extension, *profile, *toolbar_model_, *web_contents);
  menu_item.site_permissions_button = GetSitePermissionsButtonState(
      *extension, *profile, *toolbar_model_, *web_contents);
  menu_item.is_enterprise = extensions::ExtensionSystem::Get(profile)
                                ->management_policy()
                                ->HasEnterpriseForcedAccess(*extension);

  return menu_item;
}

ExtensionsMenuViewModel::OptionalSection
ExtensionsMenuViewModel::GetOptionalSection() {
  content::WebContents* web_contents = GetActiveWebContents();
  auto reload_required = [web_contents]() {
    return extensions::TabHelper::FromWebContents(web_contents)
        ->IsReloadRequired();
  };

  MainPageState state =
      GetMainPageState(*browser_->GetProfile(), *toolbar_model_, *web_contents);

  if (state == MainPageState::kUserBlockedSite) {
    return reload_required()
               ? ExtensionsMenuViewModel::OptionalSection::kReloadPage
               : ExtensionsMenuViewModel::OptionalSection::kNone;
  }

  if (state == MainPageState::kUserCustomizedSite) {
    return reload_required()
               ? ExtensionsMenuViewModel::OptionalSection::kReloadPage
               : ExtensionsMenuViewModel::OptionalSection::kHostAccessRequests;
  }

  return ExtensionsMenuViewModel::OptionalSection::kNone;
}

ExtensionsMenuViewModel::SiteSettingsState
ExtensionsMenuViewModel::GetSiteSettingsState() {
  content::WebContents* web_contents = GetActiveWebContents();
  Profile* profile = browser_->GetProfile();
  auto has_enterprise_extensions = [&]() {
    return std::any_of(
        toolbar_model_->action_ids().begin(),
        toolbar_model_->action_ids().end(),
        [profile](const ToolbarActionsModel::ActionId extension_id) {
          auto* extension = GetExtension(*profile, extension_id);
          return extensions::ExtensionSystem::Get(profile)
              ->management_policy()
              ->HasEnterpriseForcedAccess(*extension);
        });
  };

  ExtensionsMenuViewModel::SiteSettingsState site_settings;
  std::u16string current_site =
      extensions::ui_util::GetFormattedHostForDisplay(*web_contents);

  MainPageState state =
      GetMainPageState(*profile, *toolbar_model_, *web_contents);
  switch (state) {
    case MainPageState::kRestrictedSite:
      site_settings.label = l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_MENU_SITE_SETTINGS_NOT_ALLOWED_LABEL, current_site);
      site_settings.has_tooltip = false;
      site_settings.toggle.status = ControlState::Status::kHidden;
      site_settings.toggle.is_on = false;

      break;
    case MainPageState::kPolicyBlockedSite:
      site_settings.label = l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_MENU_SITE_SETTINGS_NOT_ALLOWED_LABEL, current_site);
      site_settings.has_tooltip = has_enterprise_extensions();
      site_settings.toggle.status = ControlState::Status::kHidden;
      site_settings.toggle.is_on = false;

      break;
    case MainPageState::kUserBlockedSite:
      site_settings.label = l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_MENU_SITE_SETTINGS_LABEL, current_site);
      site_settings.has_tooltip = has_enterprise_extensions();
      site_settings.toggle.status = ControlState::Status::kEnabled;
      site_settings.toggle.is_on = false;
      site_settings.toggle.tooltip_text =
          GetSiteSettingToggleText(/*is_on=*/false);

      break;
    case MainPageState::kUserCustomizedSite:
      site_settings.label = l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_MENU_SITE_SETTINGS_LABEL, current_site);
      site_settings.has_tooltip = false;
      site_settings.toggle.status = ControlState::Status::kEnabled;
      site_settings.toggle.is_on = true;
      site_settings.toggle.tooltip_text =
          GetSiteSettingToggleText(/*is_on=*/true);

      break;
  }
  return site_settings;
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

  for (Observer& observer : observers_) {
    observer.OnHostAccessRequestAddedOrUpdated(extension_id, web_contents);
  }
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
    for (Observer& observer : observers_) {
      observer.OnHostAccessRequestAddedOrUpdated(extension_id,
                                                 GetActiveWebContents());
    }
  } else {
    // Otherwise, remove the request if existent.
    for (Observer& observer : observers_) {
      observer.OnHostAccessRequestRemoved(extension_id);
    }
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

  for (Observer& observer : observers_) {
    observer.OnHostAccessRequestRemoved(extension_id);
  }
}

void ExtensionsMenuViewModel::OnHostAccessRequestsCleared(int tab_id) {
  // Ignore requests for other tabs.
  int current_tab_id =
      extensions::ExtensionTabUtil::GetTabId(GetActiveWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  for (Observer& observer : observers_) {
    observer.OnHostAccessRequestsCleared();
  }
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

  for (Observer& observer : observers_) {
    observer.OnHostAccessRequestDismissedByUser(extension_id);
  }
}

void ExtensionsMenuViewModel::OnShowAccessRequestsInToolbarChanged(
    const extensions::ExtensionId& extension_id,
    bool can_show_requests) {
  for (Observer& observer : observers_) {
    observer.OnShowHostAccessRequestsInToolbarChanged(extension_id,
                                                      can_show_requests);
  }
}

void ExtensionsMenuViewModel::OnUserPermissionsSettingsChanged(
    const extensions::PermissionsManager::UserPermissionsSettings& settings) {
  for (Observer& observer : observers_) {
    observer.OnUserPermissionsSettingsChanged();
  }
}

void ExtensionsMenuViewModel::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  for (Observer& observer : observers_) {
    observer.OnToolbarActionAdded(action_id);
  }
}

void ExtensionsMenuViewModel::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  for (Observer& observer : observers_) {
    observer.OnToolbarActionRemoved(action_id);
  }
}

void ExtensionsMenuViewModel::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  for (Observer& observer : observers_) {
    observer.OnToolbarActionUpdated();
  }
}

void ExtensionsMenuViewModel::OnToolbarModelInitialized() {
  for (Observer& observer : observers_) {
    observer.OnToolbarModelInitialized();
  }
}

void ExtensionsMenuViewModel::OnToolbarPinnedActionsChanged() {
  for (Observer& observer : observers_) {
    observer.OnToolbarPinnedActionsChanged();
  }
}

void ExtensionsMenuViewModel::OnActiveTabChanged(tabs::TabInterface* tab) {
  auto* web_contents = tab->GetContents();
  for (Observer& observer : observers_) {
    observer.OnActiveWebContentsChanged(web_contents);
  }
}

void ExtensionsMenuViewModel::DidFinishNavigation(
    content::NavigationHandle* handle) {
  auto* web_contents = GetActiveWebContents();
  for (Observer& observer : observers_) {
    observer.OnActiveWebContentsChanged(web_contents);
  }
}

content::WebContents* ExtensionsMenuViewModel::GetActiveWebContents() {
  return TabListInterface::From(browser_)->GetActiveTab()->GetContents();
}
