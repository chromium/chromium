// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin_internals_ui.h"

#include <memory>
#include <string>
#include <vector>

#include "base/hash/hash.h"
#include "base/i18n/time_formatting.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/grit/signin_internals_resources.h"
#include "components/grit/signin_internals_resources_map.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include <variant>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_debug_info.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_util.h"
#include "net/device_bound_sessions/session_display.h"
#include "net/device_bound_sessions/session_event.h"
#include "net/device_bound_sessions/session_key.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

namespace {

void CreateAndAddSignInInternalsHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISignInInternalsHost);
  webui::SetupWebUIDataSource(source, kSigninInternalsResources,
                              IDR_SIGNIN_INTERNALS_SIGNIN_INDEX_HTML);
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

bool IsGoogleSite(const net::SchemefulSite& site) {
  return site.IsSameSiteWith(GaiaUrls::GetInstance()->gaia_origin());
}

std::string GetBoundSessionExpirationString(base::Time expiration_time) {
  return (expiration_time > base::Time::Now() ? "Expires at " : "Expired at ") +
         base::TimeFormatAsIso8601(expiration_time);
}

base::ListValue GetBoundSessionFeatureStatus(Profile* profile) {
  const bool prototype_service_enabled =
      BoundSessionCookieRefreshServiceFactory::GetForProfile(profile) !=
      nullptr;
  const bool prototype_registrations_enabled =
      prototype_service_enabled &&
      switches::IsBoundSessionCredentialsEnabled(profile->GetPrefs());
  const bool prototype_multilogin_enabled =
      prototype_service_enabled &&
      base::FeatureList::IsEnabled(
          switches::kEnableOAuthMultiloginCookiesBinding);

  const bool standard_service_enabled =
      base::FeatureList::IsEnabled(net::features::kDeviceBoundSessions) &&
      profile->GetDefaultStoragePartition()->GetDeviceBoundSessionManager() !=
          nullptr;
  const bool standard_registrations_enabled =
      standard_service_enabled &&
      base::FeatureList::IsEnabled(
          net::features::kDeviceBoundSessionsForRestrictedSites);
  const bool standard_multilogin_enabled =
      standard_service_enabled &&
      base::FeatureList::IsEnabled(
          switches::kEnableOAuthMultiloginStandardCookiesBinding);

  return base::ListValue()
      .Append(
          base::DictValue()
              .Set("type", "Prototype")
              .Set("serviceEnabled", prototype_service_enabled)
              .Set("googleRegistrationsEnabled",
                   prototype_registrations_enabled)
              .Set("multiloginBindingEnabled", prototype_multilogin_enabled))
      .Append(
          base::DictValue()
              .Set("type", "Standard")
              .Set("serviceEnabled", standard_service_enabled)
              .Set("googleRegistrationsEnabled", standard_registrations_enabled)
              .Set("multiloginBindingEnabled", standard_multilogin_enabled));
}

void AppendBoundSessionInfo(
    base::DictValue& signin_status,
    Profile* profile,
    const std::map<net::device_bound_sessions::SessionKey::Id,
                   net::device_bound_sessions::SessionDisplay>&
        standard_bound_sessions) {
  // TODO(b/299884315): update bound session info dynamically by observing the
  // service.
  static constexpr std::string_view kSessionIdKey = "sessionID";
  signin_status.Set("boundSessionFeatureStatus",
                    GetBoundSessionFeatureStatus(profile));

  base::ListValue bound_sessions_list;
  if (BoundSessionCookieRefreshService* bound_session_service =
          BoundSessionCookieRefreshServiceFactory::GetForProfile(profile)) {
    for (const auto& info : bound_session_service->GetBoundSessionDebugInfo()) {
      bound_sessions_list.Append(
          base::DictValue()
              .Set(kSessionIdKey, info.session_id)
              .Set("type", "Prototype")
              .Set("domain", info.domain)
              .Set("path", info.path)
              .Set("expirationTime",
                   GetBoundSessionExpirationString(info.expiration_time))
              .Set("throttlingPaused", info.throttling_paused)
              .Set("boundCookieNames", info.bound_cookie_names)
              .Set("refreshUrl", info.refresh_url.spec()));
    }
  }

  for (const auto& [id, session] : standard_bound_sessions) {
    std::vector<std::string> cookie_names;
    base::flat_set<std::string> domains;
    base::flat_set<std::string> paths;
    for (const auto& craving : session.cookie_cravings) {
      cookie_names.push_back(craving.name);
      std::string domain = net::cookie_util::CookieDomainAsHost(craving.domain);
      if (!domain.empty()) {
        domains.insert(std::move(domain));
      }
      if (!craving.path.empty()) {
        paths.insert(craving.path);
      }
    }
    bound_sessions_list.Append(
        base::DictValue()
            .Set(kSessionIdKey, id.value())
            .Set("type", "Standard")
            .Set("domain", base::JoinString(domains, ", "))
            .Set("path", base::JoinString(paths, ", "))
            .Set("boundCookieNames", base::JoinString(cookie_names, ", "))
            .Set("refreshUrl", session.refresh_url.spec()));
  }

  if (bound_sessions_list.empty()) {
    bound_sessions_list.Append(
        base::DictValue().Set(kSessionIdKey, "No active bound sessions."));
  }
  signin_status.Set("boundSessionInfo", std::move(bound_sessions_list));
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

}  //  namespace

SignInInternalsUI::SignInInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  CreateAndAddSignInInternalsHTMLSource(Profile::FromWebUI(web_ui));
  web_ui->AddMessageHandler(std::make_unique<SignInInternalsHandler>());
}

SignInInternalsUI::~SignInInternalsUI() = default;

SignInInternalsHandler::SignInInternalsHandler() = default;

SignInInternalsHandler::~SignInInternalsHandler() {
  // This handler can be destroyed without OnJavascriptDisallowed() ever being
  // called (https://crbug.com/40055554). Call it to ensure that `this` is
  // removed as an observer.
  OnJavascriptDisallowed();
}

void SignInInternalsHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile) {
    AboutSigninInternals* about_signin_internals =
        AboutSigninInternalsFactory::GetForProfile(profile);
    if (about_signin_internals) {
      about_signin_internals_observeration_.Observe(about_signin_internals);
    }
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    if (network::mojom::DeviceBoundSessionManager* session_manager =
            profile->GetDefaultStoragePartition()
                ->GetDeviceBoundSessionManager()) {
      session_manager->AddEventObserver(
          device_bound_session_observer_receiver_.BindNewPipeAndPassRemote());
      device_bound_session_observer_receiver_.set_disconnect_handler(
          base::BindOnce(
              &SignInInternalsHandler::OnDeviceBoundSessionObserverDisconnected,
              base::Unretained(this)));
    }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  }
}

void SignInInternalsHandler::OnJavascriptDisallowed() {
  about_signin_internals_observeration_.Reset();
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  device_bound_session_observer_receiver_.reset();
  standard_bound_sessions_.clear();
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
}

void SignInInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSigninInfo",
      base::BindRepeating(&SignInInternalsHandler::HandleGetSignInInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "overrideCapability",
      base::BindRepeating(&SignInInternalsHandler::HandleOverrideCapability,
                          base::Unretained(this)));
}

void SignInInternalsHandler::HandleOverrideCapability(
    const base::ListValue& args) {
  AllowJavascript();

  CHECK_EQ(args.size(), 3u);
  std::string account_id_str = args[0].GetString();
  std::string capability_name = args[1].GetString();
  std::string value_str = args[2].GetString();

  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile) {
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  if (!about_signin_internals) {
    return;
  }

  CoreAccountId account_id = CoreAccountId::FromString(account_id_str);
  if (!about_signin_internals->CanOverrideAccountCapability(
          account_id, capability_name, chrome::GetChannel())) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return;
  }

  std::optional<signin::Tribool> override_value;

  if (value_str == "True") {
    override_value = signin::Tribool::kTrue;
  } else if (value_str == "False") {
    override_value = signin::Tribool::kFalse;
  } else if (value_str == "Unknown") {
    override_value = signin::Tribool::kUnknown;
  } else if (value_str.empty()) {
    override_value = std::nullopt;
  } else {
    NOTREACHED() << "Invalid override value: " << value_str;
  }

  identity_manager->SetCapabilityOverride(account_id, capability_name,
                                          override_value);
}

void SignInInternalsHandler::HandleGetSignInInfo(const base::ListValue& args) {
  std::string callback_id = args[0].GetString();
  AllowJavascript();

  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  // TODO(vishwath): The UI would look better if we passed in a dict with some
  // reasonable defaults, so the about:signin-internals page doesn't look
  // empty in incognito mode. Alternatively, we could force about:signin to
  // open in non-incognito mode always (like about:settings for ex.).
  base::DictValue signin_status =
      about_signin_internals ? about_signin_internals->GetSigninStatus()
                             : base::DictValue();
  AddAccountCapabilitiesOverridesInfo(signin_status);
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  AppendBoundSessionInfo(signin_status, profile, standard_bound_sessions_);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  ResolveJavascriptCallback(base::Value(callback_id), std::move(signin_status));

  if (!about_signin_internals) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar =
      identity_manager->GetAccountsInCookieJar();
  if (accounts_in_cookie_jar.AreAccountsFresh()) {
    about_signin_internals->OnAccountsInCookieUpdated(
        accounts_in_cookie_jar, GoogleServiceAuthError::AuthErrorNone());
  }
}

void SignInInternalsHandler::OnSigninStateChanged(const base::DictValue& info) {
  NotifySigninStateChanged(info.Clone());
}

void SignInInternalsHandler::NotifySigninStateChanged(
    base::DictValue signin_status) {
  AddAccountCapabilitiesOverridesInfo(signin_status);
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (Profile* profile = Profile::FromWebUI(web_ui())) {
    AppendBoundSessionInfo(signin_status, profile, standard_bound_sessions_);
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  FireWebUIListener("signin-info-changed", signin_status);
}

void SignInInternalsHandler::OnCookieAccountsFetched(
    const base::DictValue& info) {
  FireWebUIListener("update-cookie-accounts", info);
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
void SignInInternalsHandler::AddDeviceBoundSessionDisplays(
    const std::vector<net::device_bound_sessions::SessionDisplay>&
        session_displays) {
  bool updated = false;
  for (const auto& session : session_displays) {
    if (IsGoogleSite(session.key.site)) {
      standard_bound_sessions_.insert_or_assign(session.key.id, session);
      updated = true;
    }
  }
  if (updated) {
    NotifySigninStateChanged();
  }
}

void SignInInternalsHandler::OnDeviceBoundSessionEventReceived(
    const net::device_bound_sessions::SessionEvent& event) {
  if (!event.succeeded || !IsGoogleSite(event.site) ||
      !event.session_id.has_value()) {
    return;
  }
  net::device_bound_sessions::SessionKey::Id session_id(*event.session_id);
  std::visit(
      absl::Overload{
          [this, &session_id](
              const net::device_bound_sessions::CreationEventDetails& details) {
            if (details.new_session_display.has_value()) {
              standard_bound_sessions_.insert_or_assign(
                  session_id, *details.new_session_display);
              NotifySigninStateChanged();
            }
          },
          [this, &session_id](
              const net::device_bound_sessions::RefreshEventDetails& details) {
            if (details.new_session_display.has_value()) {
              standard_bound_sessions_.insert_or_assign(
                  session_id, *details.new_session_display);
              NotifySigninStateChanged();
            }
          },
          [this, &session_id](
              const net::device_bound_sessions::TerminationEventDetails&) {
            if (standard_bound_sessions_.erase(session_id) > 0) {
              NotifySigninStateChanged();
            }
          },
          [](const net::device_bound_sessions::ChallengeEventDetails&) {},
      },
      event.event_type_details);
}

void SignInInternalsHandler::NotifySigninStateChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile) {
    return;
  }
  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  NotifySigninStateChanged(about_signin_internals
                               ? about_signin_internals->GetSigninStatus()
                               : base::DictValue());
}

void SignInInternalsHandler::OnDeviceBoundSessionObserverDisconnected() {
  device_bound_session_observer_receiver_.reset();
  if (!standard_bound_sessions_.empty()) {
    standard_bound_sessions_.clear();
    NotifySigninStateChanged();
  }
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

void SignInInternalsHandler::AddAccountCapabilitiesOverridesInfo(
    base::DictValue& signin_status) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile) {
    return;
  }
  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  if (!about_signin_internals) {
    return;
  }

  base::ListValue* account_capabilities =
      signin_status.FindList("accountCapabilities");
  if (account_capabilities) {
    for (base::Value& item : *account_capabilities) {
      if (!item.is_dict()) {
        continue;
      }
      base::DictValue& account_dict = item.GetDict();
      const std::string* account_id_str = account_dict.FindString("accountId");
      if (!account_id_str) {
        continue;
      }
      CoreAccountId account_id = CoreAccountId::FromString(*account_id_str);
      base::ListValue* capabilities_list =
          account_dict.FindList("capabilities");
      if (!capabilities_list) {
        continue;
      }
      for (base::Value& cap_item : *capabilities_list) {
        if (!cap_item.is_dict()) {
          continue;
        }
        base::DictValue& cap_dict = cap_item.GetDict();
        const std::string* cap_name = cap_dict.FindString("name");
        if (!cap_name) {
          continue;
        }
        bool can_override =
            about_signin_internals->CanOverrideAccountCapability(
                account_id, *cap_name, chrome::GetChannel());
        cap_dict.Set("can_override", can_override);
      }
    }
  }
}
