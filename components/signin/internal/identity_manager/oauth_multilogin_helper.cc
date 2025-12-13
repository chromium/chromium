// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_helper.h"

#include <algorithm>
#include <utility>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_fetcher.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_response.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth_multilogin_result.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/public/base/bound_session_oauth_multilogin_delegate.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace signin {

namespace {

constexpr int kMaxFetcherRetries = 3;
static_assert(kMaxFetcherRetries > 1, "Must have at least one retry attempt");

CoreAccountId FindAccountIdForGaiaId(
    const std::vector<OAuthMultiloginHelper::AccountIdGaiaIdPair>& accounts,
    const GaiaId& gaia_id) {
  auto it = std::ranges::find(
      accounts, gaia_id, &OAuthMultiloginHelper::AccountIdGaiaIdPair::second);
  return it != accounts.end() ? it->first : CoreAccountId();
}

std::string FindTokenForAccountId(
    const base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse>& tokens,
    const CoreAccountId& account_id) {
  auto it = tokens.find(account_id);
  return it != tokens.end() ? it->second.oauth_token() : std::string();
}

net::CookieOptions GetCookieOptions() {
  net::CookieOptions options;
  options.set_include_httponly();
  // Permit it to set a SameSite cookie if it wants to.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  return options;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
net::device_bound_sessions::SessionParams
CreateStandardDeviceBoundSessionParamsFromRegistrationPayload(
    const RegisterBoundSessionPayload& registration_payload) {
  using net::device_bound_sessions::SessionParams;
  CHECK(registration_payload.parsed_for_dbsc_standard);

  std::vector<SessionParams::Scope::Specification> specifications;
  for (const RegisterBoundSessionPayload::Scope& from_spec :
       registration_payload.scope.specifications) {
    std::optional<SessionParams::Scope::Specification::Type> type;
    switch (from_spec.type) {
      case RegisterBoundSessionPayload::Scope::Type::kExclude:
        type = SessionParams::Scope::Specification::Type::kExclude;
        break;
      case RegisterBoundSessionPayload::Scope::Type::kInclude:
        type = SessionParams::Scope::Specification::Type::kInclude;
        break;
    }
    CHECK(type.has_value());
    specifications.push_back(
        {.type = *type, .domain = from_spec.domain, .path = from_spec.path});
  }

  SessionParams::Scope scope;
  scope.include_site = registration_payload.scope.include_site;
  scope.specifications = std::move(specifications);
  scope.origin = registration_payload.scope.origin;

  std::vector<SessionParams::Credential> credentials;
  for (const RegisterBoundSessionPayload::Credential& from_credential :
       registration_payload.credentials) {
    CHECK_EQ(from_credential.type, "cookie");
    credentials.push_back({.name = from_credential.name,
                           .attributes = from_credential.attributes});
  }

  return SessionParams(registration_payload.session_id,
                       // TODO(crbug.com/464268881): Use more robust URL here to
                       // support different domains.
                       GaiaUrls::GetInstance()->oauth_multilogin_url(),
                       registration_payload.refresh_url, std::move(scope),
                       std::move(credentials),
                       // Passing an arbitrary key in params as it will be
                       // retrieved later from the wrapped key passed to
                       // the `DeviceBoundSessionManager`.
                       unexportable_keys::UnexportableKeyId(),
                       registration_payload.allowed_refresh_initiators);
}

void RecordCreateBoundSessionsResult(
    OAuthMultiloginHelper::DeviceBoundSessionCreateSessionsResult result) {
  base::UmaHistogramEnumeration(
      "Signin.DeviceBoundSessions.OAuthMultilogin.CreateSessionsResult",
      result);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

OAuthMultiloginHelper::OAuthMultiloginHelper(
    SigninClient* signin_client,
    AccountsCookieMutator::PartitionDelegate* partition_delegate,
    ProfileOAuth2TokenService* token_service,
    gaia::MultiloginMode mode,
    bool wait_on_connectivity,
    const std::vector<AccountIdGaiaIdPair>& accounts,
    const std::string& external_cc_result,
    const gaia::GaiaSource& gaia_source,
    base::OnceCallback<void(SetAccountsInCookieResult)> callback)
    : signin_client_(signin_client),
      partition_delegate_(partition_delegate),
      token_service_(token_service),
      mode_(mode),
      wait_on_connectivity_(wait_on_connectivity),
      accounts_(accounts),
      external_cc_result_(external_cc_result),
      gaia_source_(gaia_source),
      callback_(std::move(callback)) {
  DCHECK(signin_client_);
  DCHECK(partition_delegate_);
  DCHECK(token_service_);
  DCHECK(!accounts_.empty());
  DCHECK(callback_);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  bound_session_delegate_ =
      partition_delegate
          ->CreateBoundSessionOAuthMultiLoginDelegateForPartition();
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#ifndef NDEBUG
  // Check that there is no duplicate accounts.
  std::set<AccountIdGaiaIdPair> accounts_no_duplicates(accounts_.begin(),
                                                       accounts_.end());
  DCHECK_EQ(accounts_.size(), accounts_no_duplicates.size());
#endif

  StartFetchingTokens();
}

OAuthMultiloginHelper::~OAuthMultiloginHelper() = default;

void OAuthMultiloginHelper::SetEphemeralKeyForTesting(
    HybridEncryptionKey ephemeral_key) {
  ephemeral_key_ = std::move(ephemeral_key);
}

void OAuthMultiloginHelper::StartFetchingTokens() {
  DCHECK(!token_fetcher_);
  DCHECK(tokens_.empty());
  std::vector<OAuthMultiloginTokenFetcher::AccountParams> account_params;
  for (const auto& account : accounts_) {
    const CoreAccountId& account_id = account.first;
    auto challenge_it = token_binding_challenges_.find(account_id);
    bool has_challenge = challenge_it != token_binding_challenges_.end();
    account_params.push_back(
        {.account_id = account_id,
         .token_binding_challenge =
             has_challenge ? challenge_it->second : std::string()});
  }

  std::string ephemeral_public_key;
  if (!token_binding_challenges_.empty()) {
    // Create a new key if we don't have one.
    if (!ephemeral_key_.has_value()) {
      ephemeral_key_.emplace();
    }
    ephemeral_public_key = ephemeral_key_->ExportPublicKey();
  }

  token_fetcher_ = std::make_unique<OAuthMultiloginTokenFetcher>(
      signin_client_, token_service_, std::move(account_params),
      std::move(ephemeral_public_key),
      base::BindOnce(&OAuthMultiloginHelper::OnMultiloginTokensSuccess,
                     base::Unretained(this)),
      base::BindOnce(&OAuthMultiloginHelper::OnMultiloginTokensFailure,
                     base::Unretained(this)),
      /*retry_waits_on_connectivity=*/wait_on_connectivity_);
}

void OAuthMultiloginHelper::OnMultiloginTokensSuccess(
    base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse> tokens) {
  CHECK(tokens_.empty());
  CHECK_EQ(tokens.size(), accounts_.size());
  tokens_ = std::move(tokens);
  token_fetcher_.reset();
  auto callback =
      base::BindOnce(&OAuthMultiloginHelper::StartFetchingMultiLogin,
                     weak_ptr_factory_.GetWeakPtr());
  if (wait_on_connectivity_) {
    signin_client_->DelayNetworkCall(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void OAuthMultiloginHelper::OnMultiloginTokensFailure(
    const GoogleServiceAuthError& error) {
  token_fetcher_.reset();
  std::move(callback_).Run(error.IsTransientError()
                               ? SetAccountsInCookieResult::kTransientError
                               : SetAccountsInCookieResult::kPersistentError);
  // Do not add anything below this line, because this may be deleted.
}

void OAuthMultiloginHelper::StartFetchingMultiLogin() {
  CHECK_EQ(tokens_.size(), accounts_.size());
  std::vector<gaia::MultiloginAccountAuthCredentials> multilogin_credentials;
  // Accounts must be listed in the same order as in `accounts_`.
  for (const auto& account : accounts_) {
    auto token_it = tokens_.find(account.first);
    CHECK(token_it != tokens_.end());
    std::string token_binding_assertion;
    token_binding_assertion = token_it->second.token_binding_assertion();

    multilogin_credentials.emplace_back(account.second,
                                        token_it->second.oauth_token(),
                                        std::move(token_binding_assertion));
  }

  OAuthMultiloginResult::CookieDecryptor decryptor;
  if (ephemeral_key_.has_value()) {
    decryptor = base::BindRepeating(&DecryptValueWithEphemeralKey,
                                    std::move(ephemeral_key_).value());
    // std::move() above doesn't invalidate `ephemeral_key_`, so call reset()
    // explicitly.
    ephemeral_key_.reset();
  }

  gaia_auth_fetcher_ = partition_delegate_->CreateGaiaAuthFetcherForPartition(
      this, gaia_source_);
  gaia::MultiloginCookieBindingParams cookie_binding_params;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  switch (GetCookieBindingSupport()) {
    case CookieBindingSupport::kStandard:
      cookie_binding_params.mode =
          gaia::MultiloginCookieBindingParams::Mode::kEnabledEnforced;
      cookie_binding_params.standard_device_bound_session_credentials = true;
      break;
    case CookieBindingSupport::kPrototype:
      if (base::FeatureList::IsEnabled(
              switches::kEnableOAuthMultiloginCookiesBindingServerExperiment)) {
        cookie_binding_params.mode =
            switches::kOAuthMultiloginCookieBindingEnforced.Get()
                ? gaia::MultiloginCookieBindingParams::Mode::kEnabledEnforced
                : gaia::MultiloginCookieBindingParams::Mode::kEnabledUnenforced;
      }
      break;
    case CookieBindingSupport::kDisabled:
      break;
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  gaia_auth_fetcher_->StartOAuthMultilogin(
      mode_, multilogin_credentials, external_cc_result_, std::move(decryptor),
      cookie_binding_params);
}

void OAuthMultiloginHelper::OnOAuthMultiloginFinished(
    const OAuthMultiloginResult& result) {
  if (result.status() == OAuthMultiloginResponseStatus::kOk) {
    if (VLOG_IS_ON(1)) {
      std::vector<std::string> account_ids;
      for (const auto& account : accounts_) {
        account_ids.push_back(account.first.ToString());
      }
      VLOG(1) << "Multilogin successful accounts="
              << base::JoinString(account_ids, " ");
    }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    switch (GetCookieBindingSupport()) {
      case CookieBindingSupport::kStandard:
        if (StartSettingCookiesViaDeviceBoundSessionManager(result)) {
          // No need to set cookies as they will be set by
          // `DeviceBoundSessionManager`.
          return;
        }
        // Fallback to the legacy cookie setting flow if setting cookies via
        // `DeviceBoundSessionManager` has not started successfully.
        break;
      case CookieBindingSupport::kPrototype:
        CHECK(bound_session_delegate_);
        bound_session_delegate_->BeforeSetCookies(result);
        break;
      case CookieBindingSupport::kDisabled:
        break;
    }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

    StartSettingCookies(result);
    return;
  }

  // If Gaia responded with kInvalidTokens or kRetryWithTokenBindingChallenge,
  // we have to mark tokens without recovery method as invalid.
  if (result.status() == OAuthMultiloginResponseStatus::kInvalidTokens ||
      result.status() ==
          OAuthMultiloginResponseStatus::kRetryWithTokenBindingChallenge) {
    for (const OAuthMultiloginResult::FailedAccount& failed_account :
         result.failed_accounts()) {
      CoreAccountId failed_account_id =
          FindAccountIdForGaiaId(accounts_, failed_account.gaia_id);
      if (failed_account_id.empty()) {
        LOG(ERROR) << "Unexpected failed gaia id for an account not present in "
                      "request: "
                   << failed_account.gaia_id;
        continue;
      }
      if (!failed_account.token_binding_challenge.empty()) {
        auto [_, inserted] = token_binding_challenges_.insert(
            {failed_account_id, failed_account.token_binding_challenge});
        if (inserted) {
          // If an account haven't received a token binding challenge before,
          // try to recover by providing a token binding assertion.
          continue;
        }
      }

      std::string failed_token =
          FindTokenForAccountId(tokens_, failed_account_id);
      CHECK(!failed_token.empty());
      token_service_->InvalidateTokenForMultilogin(failed_account_id,
                                                   failed_token);
      token_binding_challenges_.erase(failed_account_id);
    }
  }

  bool is_transient_error =
      result.status() == OAuthMultiloginResponseStatus::kInvalidTokens ||
      result.status() == OAuthMultiloginResponseStatus::kRetry ||
      result.status() ==
          OAuthMultiloginResponseStatus::kRetryWithTokenBindingChallenge;

  if (is_transient_error && ++fetcher_retries_ < kMaxFetcherRetries) {
    tokens_.clear();
    StartFetchingTokens();
    return;
  }
  std::move(callback_).Run(is_transient_error
                               ? SetAccountsInCookieResult::kTransientError
                               : SetAccountsInCookieResult::kPersistentError);
  // Do not add anything below this line, because this may be deleted.
}

void OAuthMultiloginHelper::StartSettingCookies(
    const OAuthMultiloginResult& result) {
  network::mojom::CookieManager* cookie_manager =
      partition_delegate_->GetCookieManagerForPartition();
  net::CookieInclusionStatus default_cookie_inclusion_status;
  default_cookie_inclusion_status.AddExclusionReason(
      net::CookieInclusionStatus::ExclusionReason::EXCLUDE_UNKNOWN_ERROR);

  // Set only one cookie per (name, domain) pair.
  absl::flat_hash_map<std::pair<std::string_view, std::string_view>,
                      raw_ref<const net::CanonicalCookie>>
      unique_cookies;
  for (const net::CanonicalCookie& cookie : result.cookies()) {
    unique_cookies.try_emplace({cookie.Name(), cookie.Domain()}, cookie);
  }

  base::RepeatingCallback<void(net::CookieAccessResult)> barrier_callback =
      base::BarrierCallback<net::CookieAccessResult>(
          unique_cookies.size(),
          base::BindOnce(&OAuthMultiloginHelper::OnCookiesSet,
                         weak_ptr_factory_.GetWeakPtr()));
  for (const auto& [_, cookie] : unique_cookies) {
    cookie_manager->SetCanonicalCookie(
        *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
        GetCookieOptions(),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::OnceCallback<void(net::CookieAccessResult)>(barrier_callback),
            net::CookieAccessResult(default_cookie_inclusion_status)));
  }
}

void OAuthMultiloginHelper::OnCookiesSet(
    const std::vector<net::CookieAccessResult>& results) {
  for (const auto& result : results) {
    base::UmaHistogramBoolean("Signin.SetCookieSuccess",
                              result.status.IsInclude());
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (GetCookieBindingSupport() == CookieBindingSupport::kPrototype) {
    bound_session_delegate_->OnCookiesSet();
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  std::move(callback_).Run(SetAccountsInCookieResult::kSuccess);
  // Do not add anything below this line, because `this` may be deleted.
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
OAuthMultiloginHelper::CookieBindingSupport
OAuthMultiloginHelper::GetCookieBindingSupport() const {
  if (partition_delegate_->GetDeviceBoundSessionManagerForPartition() &&
      base::FeatureList::IsEnabled(
          switches::kEnableOAuthMultiloginStandardCookiesBinding)) {
    return CookieBindingSupport::kStandard;
  }
  if (bound_session_delegate_ &&
      base::FeatureList::IsEnabled(
          switches::kEnableOAuthMultiloginCookiesBinding)) {
    return CookieBindingSupport::kPrototype;
  }
  return CookieBindingSupport::kDisabled;
}

bool OAuthMultiloginHelper::StartSettingCookiesViaDeviceBoundSessionManager(
    const OAuthMultiloginResult& result) {
  std::vector<net::device_bound_sessions::SessionParams> sessions_params;
  for (const OAuthMultiloginResult::DeviceBoundSession* device_bound_session :
       result.GetDeviceBoundSessionsToRegister()) {
    CHECK(device_bound_session);
    CHECK(device_bound_session->register_session_payload.has_value());
    sessions_params.push_back(
        CreateStandardDeviceBoundSessionParamsFromRegistrationPayload(
            *device_bound_session->register_session_payload));
  }
  if (sessions_params.empty()) {
    RecordCreateBoundSessionsResult(
        DeviceBoundSessionCreateSessionsResult::kFallbackNoBoundSessions);
    return false;
  }

  std::vector<uint8_t> wrapped_key;
  for (const AccountIdGaiaIdPair& account : accounts_) {
    wrapped_key = token_service_->GetWrappedBindingKey(account.first);
    if (!wrapped_key.empty()) {
      break;
    }
  }
  if (wrapped_key.empty()) {
    RecordCreateBoundSessionsResult(
        DeviceBoundSessionCreateSessionsResult::kFallbackNoBindingKey);
    return false;
  }

  network::mojom::DeviceBoundSessionManager* device_bound_session_manager =
      partition_delegate_->GetDeviceBoundSessionManagerForPartition();
  CHECK(device_bound_session_manager);
  device_bound_session_manager->CreateBoundSessions(
      std::move(sessions_params), wrapped_key, result.cookies(),
      GetCookieOptions(),
      base::BindOnce(&OAuthMultiloginHelper::OnBoundSessionsCreated,
                     weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void OAuthMultiloginHelper::OnBoundSessionsCreated(
    const std::vector<net::device_bound_sessions::SessionError::ErrorType>&
        session_results,
    std::vector<net::CookieInclusionStatus> cookie_results) {
  bool all_success = true;
  for (const auto& error : session_results) {
    base::UmaHistogramEnumeration(
        "Signin.DeviceBoundSessions.OAuthMultilogin.SessionCreationError",
        error);
    all_success &=
        (error ==
         net::device_bound_sessions::SessionError::ErrorType::kSuccess);
  }

  RecordCreateBoundSessionsResult(
      all_success ? DeviceBoundSessionCreateSessionsResult::kSuccess
                  : DeviceBoundSessionCreateSessionsResult::kFailure);

  for (const auto& status : cookie_results) {
    base::UmaHistogramBoolean("Signin.SetCookieSuccess", status.IsInclude());
  }

  std::move(callback_).Run(SetAccountsInCookieResult::kSuccess);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
