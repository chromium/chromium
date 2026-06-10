// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/glic_handler.h"

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_service_factory.h"
#include "chrome/browser/password_manager/factories/account_password_store_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permissions_manager_impl.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace settings {

namespace {

base::ListValue ConvertActorLoginPermissionsToList(
    base::flat_set<password_manager::ActorLoginPermission> all_permissions) {
  base::ListValue permissions_list;
  for (const password_manager::ActorLoginPermission& permission :
       all_permissions) {
    auto permission_dict =
        base::DictValue()
            .Set("signonRealm", permission.domain_info.signon_realm)
            .Set("username", permission.username)
            .Set("displayName", permission.domain_info.name)
            .Set("faviconUrl", permission.favicon_url.spec());
    permissions_list.Append(std::move(permission_dict));
  }
  return permissions_list;
}

}  // namespace

GlicHandler::GlicHandler() = default;
GlicHandler::~GlicHandler() = default;

void GlicHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setGlicOsLauncherEnabled",
      base::BindRepeating(&GlicHandler::HandleSetGlicOsLauncherEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getGlicShortcut",
      base::BindRepeating(&GlicHandler::HandleGetGlicShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setGlicShortcut",
      base::BindRepeating(&GlicHandler::HandleSetGlicShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getGlicFocusToggleShortcut",
      base::BindRepeating(&GlicHandler::HandleGetGlicFocusToggleShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setGlicFocusToggleShortcut",
      base::BindRepeating(&GlicHandler::HandleSetGlicFocusToggleShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setShortcutSuspensionState",
      base::BindRepeating(&GlicHandler::HandleSetShortcutSuspensionState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getGlicDisallowedByAdmin",
      base::BindRepeating(&GlicHandler::HandleGetGlicDisallowedByAdmin,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getActorLoginPermissions",
      base::BindRepeating(&GlicHandler::HandleGetActorLoginPermissions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "revokeActorLoginPermission",
      base::BindRepeating(&GlicHandler::HandleRevokeActorLoginPermission,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getGlicSelectionShortcut",
      base::BindRepeating(&GlicHandler::HandleGetGlicSelectionShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setGlicSelectionShortcut",
      base::BindRepeating(&GlicHandler::HandleSetGlicSelectionShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getWebActuationToggleVisibility",
      base::BindRepeating(&GlicHandler::HandleGetWebActuationToggleVisibility,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getWebActuationEnabled",
      base::BindRepeating(&GlicHandler::HandleGetWebActuationEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setWebActuationEnabled",
      base::BindRepeating(&GlicHandler::HandleSetWebActuationEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExperimentalTriggeringEnabled",
      base::BindRepeating(&GlicHandler::HandleGetExperimentalTriggeringEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setExperimentalTriggeringEnabled",
      base::BindRepeating(&GlicHandler::HandleSetExperimentalTriggeringEnabled,
                          base::Unretained(this)));
}

void GlicHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (auto* service =
          glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile)) {
    // Unretained is safe here since our subscription will expire upon our
    // destruction.
    glic_enabling_subscription_ = service->enabling().RegisterAllowedChanged(
        base::BindRepeating(&GlicHandler::FireOnGlicDisallowedByAdminChanged,
                            base::Unretained(this)));

    web_actuation_subscription_ = service->AddActOnWebCapabilityChangedCallback(
        base::BindRepeating(&GlicHandler::OnWebActuationCapabilityChanged,
                            base::Unretained(this)));

    web_actuation_pref_subscription_ =
        service->enabling().RegisterOnUserEnabledActuationOnWebChanged(
            base::BindRepeating(&GlicHandler::OnWebActuationPrefChanged,
                                base::Unretained(this)));
    experimental_triggering_pref_subscription_ =
        service->enabling().RegisterOnExperimentalTriggeringEnabledChanged(
            base::BindRepeating(
                &GlicHandler::OnExperimentalTriggeringPrefChanged,
                base::Unretained(this)));

    pref_change_registrar_.Init(profile->GetPrefs());
    pref_change_registrar_.Add(
        ::subscription_eligibility::prefs::kAiSubscriptionTier,
        base::BindRepeating(&GlicHandler::OnWebActuationPrefChanged,
                            base::Unretained(this)));
  }

  actor_login_permissions_manager_ =
      std::make_unique<actor_login::ActorLoginPermissionsManagerImpl>(
          AffiliationServiceFactory::GetForProfile(profile),
          actor_login::ActorLoginPermissionServiceFactory::GetForProfile(
              profile),
          ProfilePasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          AccountPasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS));
  observation_.Observe(actor_login_permissions_manager_.get());
}

void GlicHandler::OnJavascriptDisallowed() {
  glic_enabling_subscription_ = {};
  web_actuation_subscription_ = {};
  web_actuation_pref_subscription_ = {};
  experimental_triggering_pref_subscription_ = {};
  observation_.Reset();
  pref_change_registrar_.RemoveAll();
}

void GlicHandler::OnPermissionsChanged() {
  RequestPermissionsList(base::BindOnce(&GlicHandler::NotifyPermissionsChanged,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void GlicHandler::NotifyPermissionsChanged(base::ListValue permissions_list) {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("actor-login-permissions-changed", permissions_list);
  }
}

void GlicHandler::SetWebUIForTesting(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

void GlicHandler::HandleSetGlicOsLauncherEnabled(const base::ListValue& args) {
  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      web_ui()->GetWebContents()->GetBrowserContext(), features::kGlic);
}

void GlicHandler::HandleGetGlicShortcut(const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();
  ResolveJavascriptCallback(
      callback_id,
      base::UTF16ToUTF8(glic::GlicLauncherConfiguration::GetGlobalHotkey()
                            .GetShortcutText()));
}

void GlicHandler::HandleSetGlicShortcut(const base::ListValue& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string accelerator_string = args[1].GetString();
  g_browser_process->local_state()->SetString(glic::prefs::kGlicLauncherHotkey,
                                              accelerator_string);

  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      web_ui()->GetWebContents()->GetBrowserContext(),
      features::kGlicKeyboardShortcutNewBadge);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, base::Value());
}

void GlicHandler::HandleGetGlicFocusToggleShortcut(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();
  ResolveJavascriptCallback(
      callback_id,
      base::UTF16ToUTF8(glic::LocalHotkeyManager::GetConfigurableAccelerator(
                            glic::LocalHotkeyManager::Command::kFocusToggle)
                            .GetShortcutText()));
}

void GlicHandler::HandleSetGlicFocusToggleShortcut(
    const base::ListValue& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string accelerator_string = args[1].GetString();
  g_browser_process->local_state()->SetString(
      glic::prefs::kGlicFocusToggleHotkey, accelerator_string);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, base::Value());
}

void GlicHandler::HandleSetShortcutSuspensionState(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const bool should_suspend = args[0].GetBool();
  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  // `global_accelerator_listener` may be null on Linux Wayland builds.
  if (global_accelerator_listener) {
    global_accelerator_listener->SetShortcutHandlingSuspended(should_suspend);
  }
}

void GlicHandler::HandleGetGlicDisallowedByAdmin(const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();

  Profile* profile = Profile::FromWebUI(web_ui());
  const bool disallowed =
      glic::GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin();

  ResolveJavascriptCallback(callback_id, base::Value(disallowed));
}

void GlicHandler::HandleGetGlicSelectionShortcut(const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();
  ResolveJavascriptCallback(
      callback_id,
      base::UTF16ToUTF8(
          glic::GlicLauncherConfiguration::GetSelectionGlobalHotkey()
              .GetShortcutText()));
}

void GlicHandler::HandleSetGlicSelectionShortcut(const base::ListValue& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string accelerator_string = args[1].GetString();
  g_browser_process->local_state()->SetString(glic::prefs::kGlicSelectionHotkey,
                                              accelerator_string);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, base::Value());
}

void GlicHandler::FireOnGlicDisallowedByAdminChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
  const bool disallowed =
      glic::GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin();
  FireWebUIListener("glic-disallowed-by-admin-changed",
                    base::Value(disallowed));
}

void GlicHandler::OnWebActuationPrefChanged() {
  FireWebActuationToggleVisibilityChanged();

  Profile* profile = Profile::FromWebUI(web_ui());
  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  if (glic_service) {
    bool enabled = glic_service->enabling().GetUserEnabledActuationOnWeb();
    FireWebUIListener("glic-web-actuation-enabled-changed",
                      base::Value(enabled));
  }
}

void GlicHandler::OnExperimentalTriggeringPrefChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  if (glic_service) {
    bool enabled = glic_service->enabling().GetExperimentalTriggeringEnabled();
    FireWebUIListener("glic-experimental-triggering-enabled-changed",
                      base::Value(enabled));
  }
}

void GlicHandler::OnWebActuationCapabilityChanged(bool can_act_on_web) {
  FireWebUIListener("glic-web-actuation-capability-changed",
                    base::Value(can_act_on_web));
}

void GlicHandler::HandleGetWebActuationToggleVisibility(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();
  ResolveJavascriptCallback(
      callback_id,
      base::Value(ShouldShowWebActuationToggle(Profile::FromWebUI(web_ui()))));
}

void GlicHandler::HandleGetWebActuationEnabled(const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();

  Profile* profile = Profile::FromWebUI(web_ui());
  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  bool enabled = false;
  if (glic_service) {
    enabled = glic_service->enabling().GetUserEnabledActuationOnWeb();
  }
  ResolveJavascriptCallback(callback_id, base::Value(enabled));
}

void GlicHandler::HandleSetWebActuationEnabled(const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const bool enabled = args[0].GetBool();

  Profile* profile = Profile::FromWebUI(web_ui());
  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  if (glic_service) {
    glic_service->enabling().SetUserEnabledActuationOnWeb(enabled);
    if (!enabled) {
      glic_service->enabling().SetExperimentalTriggeringEnabled(false);
    }
  }
}

void GlicHandler::HandleGetExperimentalTriggeringEnabled(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();

  Profile* profile = Profile::FromWebUI(web_ui());
  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  bool enabled = false;
  if (glic_service) {
    enabled = glic_service->enabling().GetExperimentalTriggeringEnabled();
  }
  ResolveJavascriptCallback(callback_id, base::Value(enabled));
}

void GlicHandler::HandleSetExperimentalTriggeringEnabled(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const bool enabled = args[0].GetBool();

  Profile* profile = Profile::FromWebUI(web_ui());
  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  if (glic_service) {
    glic_service->enabling().SetExperimentalTriggeringEnabled(enabled);
  }
}

void GlicHandler::FireWebActuationToggleVisibilityChanged() {
  bool is_visible =
      GlicHandler::ShouldShowWebActuationToggle(Profile::FromWebUI(web_ui()));
  FireWebUIListener("glic-web-actuation-toggle-visibility-changed",
                    base::Value(is_visible));
}

bool GlicHandler::ShouldShowWebActuationToggle(Profile* profile) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kGlicAlwaysShowWebActuationToggle)) {
    return true;
  }
  if (!base::FeatureList::IsEnabled(features::kGlicWebActuationSetting)) {
    return false;
  }

  // If the account is ineligible, hide the toggle.
  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  if (!glic_service) {
    return false;
  }
  if (glic_service->actor_policy_checker().CannotActOnWebReason() ==
      glic::GlicActorPolicyChecker::CannotActReason::
          kAccountCapabilityIneligible) {
    return false;
  }

  bool is_managed = glic::GlicActorPolicyChecker::IsBrowserManaged(*profile);

  bool is_enterprise_account = false;
  if (auto* actor_service = actor::ActorKeyedService::Get(profile)) {
    is_enterprise_account = glic::GlicActorPolicyChecker::IsEnterpriseAccount(
        *profile, actor_service->GetJournal());
  }

  // Enterprise Case: Align toggle visibility with GlicActorPolicyChecker.
  if (is_managed || is_enterprise_account) {
    return glic_service->actor_policy_checker().CanActOnWeb();
  }

  // Google one User
  // If not managed, we check consumer subscription tiers.
  const base::flat_set<int32_t>& allowed_tiers =
      glic::GlicActorPolicyChecker::GetActorEligibleTiers();
  // If no tiers are allowed, the toggle should never be shown.
  if (allowed_tiers.empty()) {
    return false;
  }

  // NOTE: kGlicWebActuationSettingsToggle controls toggle visibility based
  // solely on subscription eligibility. If this feature is disabled, the
  // toggle remains visible only if the user has previously accepted the
  // consent card.
  if (base::FeatureList::IsEnabled(features::kGlicWebActuationSettingsToggle)) {
    // Always show the toggle for internal dogfooders, mirroring the bypass in
    // GlicActorPolicyChecker.
    if (glic::GlicEnabling::IsLikelyDogfoodClient()) {
      return true;
    }
    // Strict subscription check for external users.
    auto* subscription_service = subscription_eligibility::
        SubscriptionEligibilityServiceFactory::GetForProfile(profile);
    CHECK(subscription_service);
    return allowed_tiers.contains(
        subscription_service->GetAiSubscriptionTier());
  }
  // Show the toggle if the user has explicitly modified the preference before
  // (via accepting the consent card).
  if (!glic_service->enabling().IsUserEnabledActuationOnWebDefault()) {
    return true;
  }
  return false;
}

bool GlicHandler::ShouldShowExperimentalTriggeringToggle(Profile* profile) {
  if (!base::FeatureList::IsEnabled(features::kGlicExperimentalTriggering)) {
    return false;
  }
  if (!ShouldShowWebActuationToggle(profile)) {
    return false;
  }
  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  if (!glic_service) {
    return false;
  }
  if (!glic_service->enabling().IsExperimentalTriggeringUserControlled()) {
    return false;
  }
  return !glic_service->enabling().IsExperimentalTriggeringEnabledDefault();
}

void GlicHandler::HandleGetActorLoginPermissions(const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  AllowJavascript();
  const base::Value& callback_id = args[0];
  RequestPermissionsList(
      base::BindOnce(&GlicHandler::OnGetActorLoginPermissions,
                     weak_ptr_factory_.GetWeakPtr(), callback_id.GetString()));
}

void GlicHandler::OnGetActorLoginPermissions(std::string callback_id_str,
                                             base::ListValue permissions_list) {
  if (IsJavascriptAllowed()) {
    ResolveJavascriptCallback(base::Value(callback_id_str), permissions_list);
  }
}

void GlicHandler::HandleRevokeActorLoginPermission(
    const base::ListValue& args) {
  CHECK_EQ(3U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string* signon_realm = args[1].GetIfString();
  const std::string* username = args[2].GetIfString();
  if (signon_realm && username) {
    AllowJavascript();
    actor_login_permissions_manager_->RevokePermission(
        *signon_realm, *username,
        base::BindOnce(&GlicHandler::OnRevokeActorLoginPermission,
                       weak_ptr_factory_.GetWeakPtr(), callback_id));
  }
}

void GlicHandler::OnRevokeActorLoginPermission(std::string callback_id_str,
                                               bool success) {
  if (IsJavascriptAllowed()) {
    ResolveJavascriptCallback(base::Value(callback_id_str),
                              base::Value(success));
  }
}

void GlicHandler::RequestPermissionsList(
    base::OnceCallback<void(base::ListValue)> callback) {
  Profile* profile = Profile::FromWebUI(web_ui());
  actor_login_permissions_manager_->GetAllPermissions(
      SyncServiceFactory::GetForProfile(profile),
      base::BindOnce(&ConvertActorLoginPermissionsToList)
          .Then(std::move(callback)));
}

}  // namespace settings
