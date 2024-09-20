// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/login/login_display_host_webui.h"
#include "chrome/browser/ui/ash/login/signin_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/ash/components/login/auth/challenge_response/cert_utils.h"
#include "chromeos/ash/components/login/auth/public/auth_types.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/version/version_loader.h"
#include "components/user_manager/known_user.h"
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

GaiaCookiesData::GaiaCookiesData() = default;
GaiaCookiesData::~GaiaCookiesData() = default;
GaiaCookiesData::GaiaCookiesData(GaiaCookiesData const&) = default;

void GaiaCookiesData::TransferCookiesToUserContext(UserContext& user_context) {
  CHECK(!this->auth_code.empty());
  user_context.SetAuthCode(this->auth_code);

  if (this->gaps_cookie.has_value() && !this->gaps_cookie->empty()) {
    user_context.SetGAPSCookie(this->gaps_cookie.value());
  }
  if (this->rapt.has_value() && !this->rapt->empty()) {
    user_context.SetReauthProofToken(this->rapt.value());
  }
}

OnlineSigninArtifacts::~OnlineSigninArtifacts() = default;
OnlineSigninArtifacts::OnlineSigninArtifacts() = default;

OnlineSigninArtifacts::OnlineSigninArtifacts(OnlineSigninArtifacts&& original)
    : gaia_id(original.gaia_id),
      email(original.email),
      using_saml(original.using_saml),
      password(original.password),
      scraped_saml_passwords(std::move(original.scraped_saml_passwords)),
      services_list(std::move(original.services_list)),
      saml_password_attributes(std::move(original.saml_password_attributes)),
      sync_trusted_vault_keys(std::move(original.sync_trusted_vault_keys)),
      challenge_response_key(std::move(original.challenge_response_key)),
      cookies(original.cookies) {}

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
      std::nullopt /* server_time */, std::nullopt /* cookie_partition_key */,
      net::CookieSourceType::kOther,
      /*status=*/nullptr));
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

  return is_child ? user_manager::UserType::kChild
                  : user_manager::UserType::kRegular;
}

ChallengeResponseKeyOrError ExtractClientCertificates(
    const LoginClientCertUsageObserver&
        extension_provided_client_cert_usage_observer) {
  CHECK(extension_provided_client_cert_usage_observer.ClientCertsWereUsed());

  scoped_refptr<net::X509Certificate> saml_client_cert;
  std::vector<ChallengeResponseKey::SignatureAlgorithm> signature_algorithms;
  std::string extension_id;
  if (!extension_provided_client_cert_usage_observer.GetOnlyUsedClientCert(
          &saml_client_cert, &signature_algorithms, &extension_id)) {
    return base::unexpected(
        SigninError::kChallengeResponseAuthMultipleClientCerts);
  }
  ChallengeResponseKey challenge_response_key;
  if (!ExtractChallengeResponseKeyFromCert(
          *saml_client_cert, signature_algorithms, &challenge_response_key)) {
    return base::unexpected(
        SigninError::kChallengeResponseAuthInvalidClientCert);
  }
  challenge_response_key.set_extension_id(extension_id);
  return challenge_response_key;
}

std::unique_ptr<UserContext> BuildUserContextForGaiaSignIn(
    user_manager::UserType user_type,
    const AccountId& account_id,
    bool using_saml,
    bool using_saml_api,
    const std::string& password,
    const SamlPasswordAttributes& password_attributes,
    const std::optional<SyncTrustedVaultKeys>& sync_trusted_vault_keys,
    const std::optional<ChallengeResponseKey>& challenge_response_key) {
  std::unique_ptr<UserContext> user_context =
      std::make_unique<UserContext>(user_type, account_id);

  if (using_saml && challenge_response_key.has_value()) {
    user_context->GetMutableChallengeResponseKeys()->push_back(
        challenge_response_key.value());
  } else {
    Key key(password);
    key.SetLabel(kCryptohomeGaiaKeyLabel);
    user_context->SetKey(key);
    if (using_saml) {
      user_context->SetSamlPassword(SamlPassword{password});
    } else {
      if (!password.empty()) {
        user_context->SetGaiaPassword(GaiaPassword{password});
      }
    }
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

  return user_context;
}

AccountId GetAccountId(const std::string& authenticated_email,
                       const std::string& gaia_id,
                       const AccountType& account_type) {
  const std::string canonicalized_email =
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(authenticated_email));

  user_manager::KnownUser known_user(g_browser_process->local_state());
  const AccountId account_id =
      known_user.GetAccountId(authenticated_email, gaia_id, account_type);

  if (account_id.GetUserEmail() != canonicalized_email) {
    LOG(WARNING) << "Existing user '" << account_id.GetUserEmail()
                 << "' authenticated by alias '" << canonicalized_email << "'.";
  }

  return account_id;
}

bool IsFamilyLinkAllowed() {
  if (!features::IsFamilyLinkOnSchoolDeviceEnabled()) {
    return false;
  }

  CrosSettings* cros_settings = CrosSettings::Get();
  bool family_link_allowed = false;
  cros_settings->GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed,
                            &family_link_allowed);

  return family_link_allowed;
}

}  // namespace login

GaiaCookieRetriever::GaiaCookieRetriever(
    std::string signin_partition_name,
    login::SigninPartitionManager* signin_partition_manager,
    OnCookieTimeoutCallback on_cookie_timeout_callback,
    bool allow_empty_auth_code_for_testing)
    : signin_partition_name_(signin_partition_name),
      signin_partition_manager_(signin_partition_manager),
      on_cookie_timeout_callback_(std::move(on_cookie_timeout_callback)),
      allow_empty_auth_code_for_testing_(allow_empty_auth_code_for_testing) {}

GaiaCookieRetriever::~GaiaCookieRetriever() = default;

void GaiaCookieRetriever::RetrieveCookies(
    OnCookieRetrievedCallback on_cookie_retrieved_callback) {
  on_cookie_retrieved_callback_ = std::move(on_cookie_retrieved_callback);

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
        base::BindOnce(&GaiaCookieRetriever::OnCookieWaitTimeout,
                       weak_factory_.GetWeakPtr()));
  }

  const net::CookieOptions cookie_options =
      net::CookieOptions::MakeAllInclusive();
  cookie_manager->GetCookieList(
      GaiaUrls::GetInstance()->gaia_url(), cookie_options,
      net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&GaiaCookieRetriever::OnGetCookieListResponse,
                     weak_factory_.GetWeakPtr()));
}

void GaiaCookieRetriever::OnCookieChange(const net::CookieChangeInfo& change) {
  if (on_cookie_retrieved_callback_.has_value() &&
      on_cookie_retrieved_callback_.value()) {
    RetrieveCookies(std::move(on_cookie_retrieved_callback_.value()));
  }
}

void GaiaCookieRetriever::OnCookieWaitTimeout() {
  oauth_code_listener_.reset();
  cookie_waiting_timer_.reset();
  std::move(on_cookie_timeout_callback_).Run();
}

void GaiaCookieRetriever::OnGetCookieListResponse(
    const net::CookieAccessResultList& cookies,
    const net::CookieAccessResultList& excluded_cookies) {
  login::GaiaCookiesData cookie_data;
  for (const auto& cookie_with_access_result : cookies) {
    const auto& cookie = cookie_with_access_result.cookie;
    if (cookie.Name() == login::kOAUTHCodeCookie)
      cookie_data.auth_code = cookie.Value();
    else if (cookie.Name() == login::kGAPSCookie)
      cookie_data.gaps_cookie = cookie.Value();
    else if (cookie.Name() == login::kRAPTCookie)
      cookie_data.rapt = cookie.Value();
  }

  if (cookie_data.auth_code.empty() && !allow_empty_auth_code_for_testing_) {
    // Will try again from onCookieChange.

    // TODO(crbug.com/40805389): Logging as "WARNING" to make sure it's
    // preserved in the logs.
    LOG(WARNING) << "OAuth cookie empty, still waiting";
    return;
  }

  oauth_code_listener_.reset();
  cookie_waiting_timer_.reset();
  std::move(on_cookie_retrieved_callback_.value()).Run(cookie_data);
}

}  // namespace ash
