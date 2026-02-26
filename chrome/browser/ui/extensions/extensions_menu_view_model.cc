// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/i18n/case_conversion.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_delegate_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

// Returns true if the action name for `a` comes before the action name for `b`
// in an alphabetical, case-insensitive comparison.
bool SortActionsByName(const std::unique_ptr<ExtensionActionViewModel>& a,
                       const std::unique_ptr<ExtensionActionViewModel>& b) {
  return base::i18n::ToLower(a->GetActionName()) <
         base::i18n::ToLower(b->GetActionName());
}

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

std::u16string GetContextMenuAccessibleName(
    bool is_pinned,
    const std::u16string& extension_name) {
  int tooltip_id =
      is_pinned
          ? IDS_EXTENSIONS_MENU_EXTENSION_CONTEXT_MENU_BUTTON_PINNED_ACCESSIBLE_NAME
          : IDS_EXTENSIONS_MENU_EXTENSION_CONTEXT_MENU_BUTTON_ACCESSIBLE_NAME;
  return l10n_util::GetStringFUTF16(tooltip_id, extension_name);
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

std::string_view GetCurrentSiteAccessCrashValue(
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

std::string_view GetCurrentSiteInteractionCrashValue(
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

ExtensionsMenuViewModel::ExtensionSitePermissionsState::
    ExtensionSitePermissionsState() = default;
ExtensionsMenuViewModel::ExtensionSitePermissionsState::
    ExtensionSitePermissionsState(const ExtensionSitePermissionsState&) =
        default;
ExtensionsMenuViewModel::ExtensionSitePermissionsState&
ExtensionsMenuViewModel::ExtensionSitePermissionsState::operator=(
    const ExtensionSitePermissionsState&) = default;
ExtensionsMenuViewModel::ExtensionSitePermissionsState::
    ~ExtensionSitePermissionsState() = default;

ExtensionsMenuViewModel::MenuEntryState::MenuEntryState() = default;
ExtensionsMenuViewModel::MenuEntryState::MenuEntryState(
    const MenuEntryState& other) = default;
ExtensionsMenuViewModel::MenuEntryState&
ExtensionsMenuViewModel::MenuEntryState::operator=(const MenuEntryState&) =
    default;
ExtensionsMenuViewModel::MenuEntryState::~MenuEntryState() = default;

ExtensionsMenuViewModel::ExtensionsMenuViewModel(
    BrowserWindowInterface* browser,
    Delegate* delegate)
    : browser_(browser),
      delegate_(delegate),
      toolbar_model_(ToolbarActionsModel::Get(browser_->GetProfile())) {
  content::WebContentsObserver::Observe(GetActiveWebContents());
  permissions_manager_observation_.Observe(
      extensions::PermissionsManager::Get(browser_->GetProfile()));
  toolbar_model_observation_.Observe(toolbar_model_.get());
  auto* tab_list = TabListInterface::From(browser);
  tab_list_interface_observation_.Observe(tab_list);

  if (toolbar_model_->actions_initialized()) {
    Populate();
  }
}

ExtensionsMenuViewModel::~ExtensionsMenuViewModel() {
  // Stop observing to avoid notifications during destruction.
  WebContentsObserver::Observe(nullptr);
  tab_list_interface_observation_.Reset();
  toolbar_model_observation_.Reset();
  permissions_manager_observation_.Reset();
}

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

void ExtensionsMenuViewModel::ExecuteAction(
    const extensions::ExtensionId& extension_id) {
  ExtensionActionViewModel* action_model = GetActionViewModel(extension_id);
  if (!action_model) {
    return;
  }

  action_model->ExecuteUserAction(
      ToolbarActionViewModel::InvocationSource::kMenuEntry);
  base::RecordAction(
      base::UserMetricsAction("Extensions.Toolbar.ExtensionActivatedFromMenu"));
}

void ExtensionsMenuViewModel::UpdateSiteSetting(
    extensions::PermissionsManager::UserSiteSetting site_setting) {
  content::WebContents* web_contents = GetActiveWebContents();
  const url::Origin& origin =
      GetActiveWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  if (origin.opaque()) {
    return;
  }

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

bool ExtensionsMenuViewModel::CanShowSitePermissionsPage(
    const extensions::ExtensionId& extension_id) {
  content::WebContents* web_contents = GetActiveWebContents();
  Profile* profile = browser_->GetProfile();
  ExtensionActionViewModel* action_model = GetActionViewModel(extension_id);
  CHECK(action_model);
  const extensions::Extension* extension = action_model->GetExtension();
  CHECK(extension);

  return CanUserCustomizeExtensionSiteAccess(*extension, *profile,
                                             *toolbar_model_, *web_contents);
}

ExtensionActionViewModel* ExtensionsMenuViewModel::GetActionViewModel(
    const extensions::ExtensionId& extension_id) const {
  auto it =
      std::ranges::find_if(action_models_, [&extension_id](const auto& model) {
        return model->GetId() == extension_id;
      });
  return it != action_models_.end() ? it->get() : nullptr;
}

ExtensionsMenuViewModel::ControlState
ExtensionsMenuViewModel::GetActionButtonState(
    const extensions::ExtensionId& extension_id,
    const gfx::Size& icon_size) {
  ExtensionActionViewModel* action_model = GetActionViewModel(extension_id);
  CHECK(action_model);
  content::WebContents* web_contents = GetActiveWebContents();

  ExtensionsMenuViewModel::ControlState button_state;
  button_state.text = action_model->GetActionName();
  button_state.tooltip_text = action_model->GetTooltip(web_contents);
  button_state.status =
      action_model->IsEnabled(web_contents)
          ? ExtensionsMenuViewModel::ControlState::Status::kEnabled
          : ExtensionsMenuViewModel::ControlState::Status::kDisabled;
  button_state.icon = action_model->GetIcon(web_contents, icon_size);
  return button_state;
}

ui::ImageModel ExtensionsMenuViewModel::GetActionIcon(
    int action_index,
    const gfx::Size& icon_size) {
  CHECK_GE(action_index, 0);
  CHECK_LT(static_cast<size_t>(action_index), action_models_.size());
  content::WebContents* web_contents = GetActiveWebContents();

  return action_models_[action_index]->GetIcon(web_contents, icon_size);
}

ExtensionsMenuViewModel::ControlState
ExtensionsMenuViewModel::GetContextMenuButtonState(
    const extensions::ExtensionId& extension_id) {
  ExtensionActionViewModel* action_model = GetActionViewModel(extension_id);
  CHECK(action_model);

  return GetContextMenuButtonState(action_model);
}

ExtensionsMenuViewModel::HostAccessRequest
ExtensionsMenuViewModel::GetHostAccessRequest(
    const extensions::ExtensionId& extension_id,
    const gfx::Size& icon_size) {
  ExtensionActionViewModel* action_model = GetActionViewModel(extension_id);
  HostAccessRequest request;
  request.extension_id = extension_id;
  request.extension_name = action_model->GetActionName();
  request.extension_icon =
      action_model->GetIcon(GetActiveWebContents(), icon_size);

  return request;
}

ExtensionsMenuViewModel::ControlState
ExtensionsMenuViewModel::GetContextMenuButtonState(
    ExtensionActionViewModel* action_model) {
  bool is_action_pinned = toolbar_model_->IsActionPinned(action_model->GetId());
  ControlState button_state;
  button_state.accessible_name = GetContextMenuAccessibleName(
      is_action_pinned, action_model->GetActionName());
  // The action's pinned state will be used to determine the button's icon.
  // TODO(crbug.com/449814184): compute and return the icons on the model,
  // rather than having the View compute them given `is_on`.
  button_state.is_on = is_action_pinned;

  return button_state;
}

ExtensionsMenuViewModel::ExtensionSitePermissionsState
ExtensionsMenuViewModel::GetExtensionSitePermissionsState(
    const extensions::ExtensionId& extension_id,
    const gfx::Size& icon_size) {
  Profile* profile = browser_->GetProfile();
  auto* permissions_manager = PermissionsManager::Get(profile);

  ExtensionActionViewModel* action_model = GetActionViewModel(extension_id);
  const extensions::Extension* extension = action_model->GetExtension();
  CHECK(extension);
  content::WebContents* web_contents = GetActiveWebContents();
  const GURL& url = web_contents->GetLastCommittedURL();

  // Extension's site permissions can only be computed when site access can be
  // modified by the user.
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

  ExtensionSitePermissionsState extension_site_permissions;
  extension_site_permissions.extension_name = action_model->GetActionName();
  extension_site_permissions.extension_icon =
      action_model->GetIcon(web_contents, icon_size);
  extension_site_permissions.on_click_option = on_click_option;
  extension_site_permissions.on_site_option = on_site_option;
  extension_site_permissions.on_all_sites_option = on_all_sites_option;
  extension_site_permissions.show_requests_toggle =
      GetExtensionShowRequestsToggleState(extension_id);

  return extension_site_permissions;
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

ExtensionsMenuViewModel::MenuEntryState
ExtensionsMenuViewModel::GetMenuEntryState(
    const extensions::ExtensionId& extension_id,
    const gfx::Size& action_icon_size) {
  Profile* profile = browser_->GetProfile();
  ExtensionActionViewModel* action_model = GetActionViewModel(extension_id);
  const extensions::Extension* extension = action_model->GetExtension();
  CHECK(extension);
  content::WebContents* web_contents = GetActiveWebContents();

  MenuEntryState entry_state;
  entry_state.extension_id = extension_id;
  entry_state.action_button =
      GetActionButtonState(extension_id, action_icon_size);
  entry_state.context_menu_button = GetContextMenuButtonState(action_model);
  entry_state.site_access_toggle = GetSiteAccessToggleState(
      *extension, *profile, *toolbar_model_, *web_contents);
  entry_state.site_permissions_button = GetSitePermissionsButtonState(
      *extension, *profile, *toolbar_model_, *web_contents);
  entry_state.is_enterprise = extensions::ExtensionSystem::Get(profile)
                                  ->management_policy()
                                  ->HasEnterpriseForcedAccess(*extension);

  return entry_state;
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
  ExtensionsMenuViewModel::SiteSettingsState site_settings;
  if (!web_contents) {
    site_settings.toggle.status = ControlState::Status::kHidden;
    return site_settings;
  }

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

  // Ignore if the extension already has an active request.
  if (std::ranges::find(host_access_requests_, extension_id) !=
      host_access_requests_.end()) {
    return;
  }

  AddHostAccessRequest(extension_id);
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
  bool is_active =
      permissions_manager->HasActiveHostAccessRequest(tab_id, extension_id);
  bool is_on_menu_model =
      std::ranges::find(host_access_requests_, extension_id) !=
      host_access_requests_.end();

  if (is_active && is_on_menu_model) {
    // Since it's already on the menu model, just notify the observers about the
    // update.
    auto it = std::ranges::find(host_access_requests_, extension_id);
    int index = std::distance(host_access_requests_.begin(), it);

    for (Observer& observer : observers_) {
      observer.OnHostAccessRequestUpdated(extension_id, index);
    }
    return;
  }

  if (is_active && !is_on_menu_model) {
    AddHostAccessRequest(extension_id);
    return;
  }

  if (!is_active && is_on_menu_model) {
    RemoveHostAccessRequest(extension_id);
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

  RemoveHostAccessRequest(extension_id);
}

void ExtensionsMenuViewModel::OnHostAccessRequestsCleared(int tab_id) {
  // Ignore requests for other tabs.
  int current_tab_id =
      extensions::ExtensionTabUtil::GetTabId(GetActiveWebContents());
  if (tab_id != current_tab_id) {
    return;
  }

  host_access_requests_.clear();

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

  RemoveHostAccessRequest(extension_id);
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
  std::unique_ptr<ExtensionActionViewModel> action_model =
      delegate_->CreateActionViewModel(action_id);
  ExtensionActionViewModel* action_model_ptr = action_model.get();

  // Register action icon observer.
  action_icon_subscriptions_[action_id] =
      action_model->RegisterIconUpdateObserver(
          base::BindRepeating(&ExtensionsMenuViewModel::OnActionIconUpdated,
                              base::Unretained(this), action_id));

  // Insert action model in the correct order.
  auto it = std::upper_bound(action_models_.begin(), action_models_.end(),
                             action_model, SortActionsByName);
  size_t index = std::distance(action_models_.begin(), it);
  action_models_.insert(it, std::move(action_model));

  // Notify observers.
  for (Observer& observer : observers_) {
    observer.OnActionAdded(action_model_ptr, index);
  }
}

void ExtensionsMenuViewModel::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  // Find the action model and return if it doesn't exist.
  auto it = std::ranges::find_if(
      action_models_,
      [&action_id](const auto& model) { return model->GetId() == action_id; });
  if (it == action_models_.end()) {
    return;
  }

  // Calculate index for action to be removed.
  int index = std::distance(action_models_.begin(), it);

  // Move the action model out of the vector but keep it alive locally.
  // This removes it from the list (so repopulation doesn't see it)
  // but prevents immediate destruction (so Views holding a raw_ptr to it don't
  // crash).
  std::unique_ptr<ExtensionActionViewModel> preserved_action_model =
      std::move(*it);
  action_models_.erase(it);

  // Remove the action icon observer subscription.
  action_icon_subscriptions_.erase(action_id);

  // Notify observers.
  for (Observer& observer : observers_) {
    observer.OnActionRemoved(action_id, index);
  }

  // preserved_action_model goes out of scope here and is destroyed safely.
}

void ExtensionsMenuViewModel::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  // Action updates can be triggered during WebContents destruction/navigation.
  // We ignore these here as they are handled by the specific web contents
  // observers.
  if (!GetActiveWebContents()) {
    return;
  }

  // Re-sort the models in case the action name changed (affecting alphabetical
  // order).
  // TODO(emiliapaz): Investigate whether this is necessary, because extension
  // name is set on the manifest and shouldn't dynamically change.
  std::sort(action_models_.begin(), action_models_.end(), SortActionsByName);

  // Notify observers.
  for (Observer& observer : observers_) {
    observer.OnActionUpdated(action_id);
  }
}

void ExtensionsMenuViewModel::OnToolbarModelInitialized() {
  Populate();

  for (Observer& observer : observers_) {
    observer.OnActionsInitialized();
  }
}

void ExtensionsMenuViewModel::OnToolbarPinnedActionsChanged() {
  for (Observer& observer : observers_) {
    observer.OnToolbarPinnedActionsChanged();
  }
}

void ExtensionsMenuViewModel::OnActiveTabChanged(TabListInterface& tab_list,
                                                 tabs::TabInterface* tab) {
  if (!tab_list_interface_observation_.IsObserving()) {
    return;
  }
  auto* web_contents = tab->GetContents();
  WebContentsObserver::Observe(web_contents);

  OnWebContentsChanged(web_contents);
}
void ExtensionsMenuViewModel::OnTabListDestroyed(TabListInterface& tab_list) {
  tab_list_interface_observation_.Reset();
}

void ExtensionsMenuViewModel::DidFinishNavigation(
    content::NavigationHandle* handle) {
  auto* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return;
  }

  OnWebContentsChanged(web_contents);
}

void ExtensionsMenuViewModel::Populate() {
  CHECK(toolbar_model_->actions_initialized());
  CHECK(action_models_.empty());
  CHECK(host_access_requests_.empty());

  is_populated_ = true;

  // Create and sort the action models by name.
  for (const auto& id : toolbar_model_->action_ids()) {
    auto model = delegate_->CreateActionViewModel(id);
    if (model) {
      action_models_.push_back(std::move(model));
    }
  }
  std::sort(action_models_.begin(), action_models_.end(), SortActionsByName);

  UpdateHostAccessRequests();
}

void ExtensionsMenuViewModel::AddHostAccessRequest(
    const extensions::ExtensionId& extension_id) {
  // Find the "rank" of the new extension in the sorted `action_models_` list.
  auto action_model_it =
      std::ranges::find_if(action_models_, [&extension_id](const auto& model) {
        return model->GetId() == extension_id;
      });
  CHECK(action_model_it != action_models_.end());

  // Find the correct insertion spot in `host_access_requests_` to match
  // the order in `action_models_`.
  auto insert_it = host_access_requests_.begin();
  for (; insert_it != host_access_requests_.end(); ++insert_it) {
    auto current_req_it = std::ranges::find_if(
        action_models_, [req_id = *insert_it](const auto& model) {
          return model->GetId() == req_id;
        });

    if (action_model_it < current_req_it) {
      break;
    }
  }

  // Insert the extension to the requests list.
  insert_it = host_access_requests_.insert(insert_it, extension_id);

  // Notify observers.
  int index = std::distance(host_access_requests_.begin(), insert_it);
  for (Observer& observer : observers_) {
    observer.OnHostAccessRequestAdded(extension_id, index);
  }
}

void ExtensionsMenuViewModel::RemoveHostAccessRequest(
    const extensions::ExtensionId& extension_id) {
  auto it = std::ranges::find(host_access_requests_, extension_id);
  if (it == host_access_requests_.end()) {
    return;
  }

  int index = std::distance(host_access_requests_.begin(), it);
  host_access_requests_.erase(it);

  for (Observer& observer : observers_) {
    observer.OnHostAccessRequestRemoved(extension_id, index);
  }
}

void ExtensionsMenuViewModel::UpdateHostAccessRequests() {
  host_access_requests_.clear();

  // Store the extension ids for the actions that have an active request.
  // Since action_models_ is sorted, iterating through it ensures
  // host_access_requests_ is also sorted.
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser_->GetProfile());
  int tab_id = extensions::ExtensionTabUtil::GetTabId(GetActiveWebContents());

  for (const auto& action_model : action_models()) {
    auto extension_id = action_model->GetId();
    if (permissions_manager->HasActiveHostAccessRequest(tab_id, extension_id)) {
      host_access_requests_.push_back(extension_id);
    }
  }
}

void ExtensionsMenuViewModel::OnActionIconUpdated(
    const extensions::ExtensionId& extension_id) {
  // Notify observers that the action icon has changed. The platform-specific
  // delegate will then re-fetch the necessary state (e.g. MenuEntryState) and
  // update the corresponding views.
  for (Observer& observer : observers_) {
    observer.OnActionIconUpdated(extension_id);
  }
}

void ExtensionsMenuViewModel::OnWebContentsChanged(
    content::WebContents* web_contents) {
  // Host access requests are dependent on the web content's origin. Therefore,
  // we need to reset them when web contents change.
  UpdateHostAccessRequests();

  for (Observer& observer : observers_) {
    observer.OnPageNavigation();
  }
}

content::WebContents* ExtensionsMenuViewModel::GetActiveWebContents() {
  auto* tab_list = TabListInterface::From(browser_);
  if (!tab_list) {
    return nullptr;
  }
  auto* tab = tab_list->GetActiveTab();
  return tab ? tab->GetContents() : nullptr;
}
