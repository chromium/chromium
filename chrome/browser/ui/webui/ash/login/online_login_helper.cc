// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/online_login_helper.h"

#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host_webui.h"
#include "chrome/browser/ash/login/ui/signin_ui.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/ash/components/login/auth/challenge_response/cert_utils.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/version/version_loader.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace login {
namespace {

const char kGAPSCookie[] = "GAPS";
const char kOAUTHCodeCookie[] = "oauth_code";
const char kRAPTCookie[] = "RAPT";
constexpr base::TimeDelta kCookieDelay = base::Seconds(20);

}  // namespace

GaiaContext::GaiaContext() {}
GaiaContext::GaiaContext(GaiaContext const&) = default;

bool ExtractSamlPasswordAttributesEnabled() {
  return base::FeatureList::IsEnabled(::features::kInSessionPasswordChange);
}

base::OnceClosure GetStartSigninSession(::content::WebUI* web_ui,
                                        LoadGaiaWithPartition callback) {
  // Start a new session with SigninPartitionManager, generating a unique
  // StoragePartition.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui));

  auto partition_call =
      base::BindOnce(&login::SigninPartitionManager::StartSigninSession,
                     base::Unretained(signin_partition_manager),
                     web_ui->GetWebContents(), std::move(callback));
  return partition_call;
}

void SetCookieForPartition(
    const login::GaiaContext& context,
    login::SigninPartitionManager* signin_partition_manager,
    OnSetCookieForLoadGaiaWithPartition callback) {
  content::StoragePartition* partition =
      signin_partition_manager->GetCurrentStoragePartition();
  if (!partition)
    return;

  // Note: The CanonicalCookie created here is not Secure. This is fine because
  // it's being set into a different StoragePartition than the user's actual
  // profile. The SetCanonicalCookie call will succeed regardless of the scheme
  // of |gaia_url| since there are no scheme restrictions since the cookie is
  // not Secure, and there is no preexisting Secure cookie in the profile that
  // would preclude updating it insecurely. |gaia_url| is usually secure, and
  // only insecure in local testing.

  std::string gaps_cookie_value(kGAPSCookie);
  gaps_cookie_value += "=" + context.gaps_cookie;
  const GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
      gaia_url, gaps_cookie_value, base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  if (!cc)
    return;

  const net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  partition->GetCookieManagerForBrowserProcess()->SetCanonicalCookie(
      *cc.get(), gaia_url, options, std::move(callback));
}

user_manager::UserType GetUsertypeFromServicesString(
    const ::login::StringList& services) {
  bool is_child = false;
  const bool support_usm =
      base::FeatureList::IsEnabled(::features::kCrOSEnableUSMUserService);
  using KnownFlags = base::flat_set<std::string>;
  const KnownFlags known_flags =
      support_usm ? KnownFlags({"uca", "usm"}) : KnownFlags({"uca"});

  for (const std::string& item : services) {
    if (known_flags.find(item) != known_flags.end()) {
      is_child = true;
      break;
    }
  }

  return is_child ? user_manager::USER_TYPE_CHILD
                  : user_manager::USER_TYPE_REGULAR;
}

bool BuildUserContextForGaiaSignIn(
    user_manager::UserType user_type,
    const AccountId& account_id,
    bool using_saml,
    bool using_saml_api,
    const std::string& password,
    const SamlPasswordAttributes& password_attributes,
    const absl::optional<SyncTrustedVaultKeys>& sync_trusted_vault_keys,
    const LoginClientCertUsageObserver&
        extension_provided_client_cert_usage_observer,
    UserContext* user_context,
    SigninError* error) {
  *user_context = UserContext(user_type, account_id);
  if (using_saml &&
      extension_provided_client_cert_usage_observer.ClientCertsWereUsed()) {
    scoped_refptr<net::X509Certificate> saml_client_cert;
    std::vector<ChallengeResponseKey::SignatureAlgorithm> signature_algorithms;
    std::string extension_id;
    if (!extension_provided_client_cert_usage_observer.GetOnlyUsedClientCert(
            &saml_client_cert, &signature_algorithms, &extension_id)) {
      if (error)
        *error = SigninError::kChallengeResponseAuthMultipleClientCerts;
      return false;
    }
    ChallengeResponseKey challenge_response_key;
    if (!ExtractChallengeResponseKeyFromCert(
            *saml_client_cert, signature_algorithms, &challenge_response_key)) {
      if (error)
        *error = SigninError::kChallengeResponseAuthInvalidClientCert;
      return false;
    }
    challenge_response_key.set_extension_id(extension_id);
    user_context->GetMutableChallengeResponseKeys()->push_back(
        challenge_response_key);
  } else {
    Key key(password);
    key.SetLabel(kCryptohomeGaiaKeyLabel);
    user_context->SetKey(key);
    user_context->SetPasswordKey(Key(password));
  }
  user_context->SetAuthFlow(using_saml
                                ? UserContext::AUTH_FLOW_GAIA_WITH_SAML
                                : UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  if (using_saml) {
    user_context->SetIsUsingSamlPrincipalsApi(using_saml_api);
    if (ExtractSamlPasswordAttributesEnabled()) {
      user_context->SetSamlPasswordAttributes(password_attributes);
    }
  }

  if (sync_trusted_vault_keys.has_value()) {
    user_context->SetSyncTrustedVaultKeys(*sync_trusted_vault_keys);
  }

  return true;
}

}  // namespace login

OnlineLoginHelper::OnlineLoginHelper(
    std::string signin_partition_name,
    login::SigninPartitionManager* signin_partition_manager,
    OnCookieTimeoutCallback on_cookie_timeout_callback,
    CompleteLoginCallback complete_login_callback)
    : signin_partition_name_(signin_partition_name),
      signin_partition_manager_(signin_partition_manager),
      on_cookie_timeout_callback_(std::move(on_cookie_timeout_callback)),
      complete_login_callback_(std::move(complete_login_callback)) {}

OnlineLoginHelper::~OnlineLoginHelper() = default;

void OnlineLoginHelper::SetUserContext(
    std::unique_ptr<UserContext> pending_user_context) {
  pending_user_context_ = std::move(pending_user_context);
}

void OnlineLoginHelper::RequestCookiesAndCompleteAuthentication() {
  content::StoragePartition* partition =
      signin_partition_manager_->GetCurrentStoragePartition();
  if (!partition)
    return;

  // Validity check that partition did not change during login flow.
  DCHECK_EQ(signin_partition_manager_->GetCurrentStoragePartitionName(),
            signin_partition_name_);

  network::mojom::CookieManager* cookie_manager =
      partition->GetCookieManagerForBrowserProcess();
  if (!oauth_code_listener_.is_bound()) {
    // Set listener before requesting the cookies to avoid race conditions.
    cookie_manager->AddCookieChangeListener(
        GaiaUrls::GetInstance()->gaia_url(), login::kOAUTHCodeCookie,
        oauth_code_listener_.BindNewPipeAndPassRemote());
    cookie_waiting_timer_ = std::make_unique<base::OneShotTimer>();
    cookie_waiting_timer_->Start(
        FROM_HERE, login::kCookieDelay,
        base::BindOnce(&OnlineLoginHelper::OnCookieWaitTimeout,
                       weak_factory_.GetWeakPtr()));
  }

  const net::CookieOptions cookie_options =
      net::CookieOptions::MakeAllInclusive();
  cookie_manager->GetCookieList(
      GaiaUrls::GetInstance()->gaia_url(), cookie_options,
      net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&OnlineLoginHelper::OnGetCookiesForCompleteAuthentication,
                     weak_factory_.GetWeakPtr()));
}

void OnlineLoginHelper::OnCookieChange(const net::CookieChangeInfo& change) {
  RequestCookiesAndCompleteAuthentication();
}

void OnlineLoginHelper::OnCookieWaitTimeout() {
  DCHECK(pending_user_context_);
  pending_user_context_.reset();
  oauth_code_listener_.reset();
  cookie_waiting_timer_.reset();
  std::move(on_cookie_timeout_callback_).Run();
}

void OnlineLoginHelper::OnGetCookiesForCompleteAuthentication(
    const net::CookieAccessResultList& cookies,
    const net::CookieAccessResultList& excluded_cookies) {
  std::string auth_code, gaps_cookie, rapt;
  for (const auto& cookie_with_access_result : cookies) {
    const auto& cookie = cookie_with_access_result.cookie;
    if (cookie.Name() == login::kOAUTHCodeCookie)
      auth_code = cookie.Value();
    else if (cookie.Name() == login::kGAPSCookie)
      gaps_cookie = cookie.Value();
    else if (cookie.Name() == login::kRAPTCookie)
      rapt = cookie.Value();
  }

  if (auth_code.empty()) {
    // Will try again from onCookieChange.
    return;
  }

  DCHECK(pending_user_context_);
  auto user_context = std::move(pending_user_context_);
  pending_user_context_.reset();
  oauth_code_listener_.reset();
  cookie_waiting_timer_.reset();

  user_context->SetAuthCode(auth_code);
  if (!gaps_cookie.empty())
    user_context->SetGAPSCookie(gaps_cookie);
  if (!rapt.empty())
    user_context->SetReauthProofToken(rapt);

  std::move(complete_login_callback_).Run(std::move(user_context));
}

}  // namespace ash
