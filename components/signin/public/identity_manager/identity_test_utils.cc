// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_test_utils.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_ANDROID)
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_android.h"
#include "components/signin/public/android/test_support_jni_headers/AccountManagerFacadeUtil_jni.h"
#endif

namespace signin {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
// Whether identity_test_utils uses `AccountManagerFacade` or
// `ProfileOAuth2TokenService` for managing credentials.
bool ShouldUseAccountManagerFacade(IdentityManager* identity_manager) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If account consistency is `kMirror` - use `AccountManagerFacade` for
  // managing credentials, otherwise use `ProfileOAuth2TokenService`.
  return identity_manager->GetAccountConsistency() ==
         AccountConsistencyMethod::kMirror;
#else
  // In Ash - always use `AccountManagerFacade` for managing credentials.
  return true;
#endif
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Helper function that updates the refresh token for |account_id| to
// |new_token|. Before updating the refresh token, blocks until refresh tokens
// are loaded. After updating the token, blocks until the update is processed by
// |identity_manager|.
void UpdateRefreshTokenForAccount(
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    const std::string& new_token,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    const std::vector<uint8_t> wrapped_binding_key,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    signin_metrics::SourceForRefreshTokenOperation source =
        signin_metrics::SourceForRefreshTokenOperation::kUnknown) {
  DCHECK_EQ(account_tracker_service->GetAccountInfo(account_id).account_id,
            account_id)
      << "To set the refresh token for an unknown account, use "
         "MakeAccountAvailable()";

  // Ensure that refresh tokens are loaded; some platforms enforce the invariant
  // that refresh token mutation cannot occur until refresh tokens are loaded,
  // and it is desired to eventually enforce that invariant across all
  // platforms.
  WaitForRefreshTokensLoaded(identity_manager);

  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  TestIdentityManagerObserver token_updated_observer(identity_manager);
  token_updated_observer.SetOnRefreshTokenUpdatedCallback(
      run_loop.QuitClosure());

  // TODO(crbug.com/40776160): simplify this when all Lacros Profiles use
  // Mirror.
#if BUILDFLAG(IS_CHROMEOS)
  if (ShouldUseAccountManagerFacade(identity_manager)) {
    const AccountInfo& account_info =
        account_tracker_service->GetAccountInfo(account_id);
    account_manager::Account account{
        account_manager::AccountKey{account_info.gaia,
                                    account_manager::AccountType::kGaia},
        account_info.email};
    GetAccountManagerFacade(identity_manager)
        ->UpsertAccountForTesting(account, new_token);
  } else
#endif  // BUILDFLAG(IS_CHROMEOS)
  {
    token_service->UpdateCredentials(account_id, new_token, source
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                                     ,
                                     wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    );
  }

  run_loop.Run();
}

// Helper for `WaitForErrorStateOfRefreshTokenUpdatedForAccount` - checks the
// current error status using a predicate and calls quit_closure if the
// predicate returns `true`.
void CompareErrorStatusAndCallClosure(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    base::RepeatingCallback<bool(const GoogleServiceAuthError&)> predicate,
    const base::RepeatingClosure& quit_closure) {
  GoogleServiceAuthError error =
      identity_manager->GetErrorStateOfRefreshTokenForAccount(account_id);
  if (predicate.Run(error))
    quit_closure.Run();
}

}  // namespace

// --- AccountAvailabilityOptions ----------------------------------------------

AccountAvailabilityOptions::AccountAvailabilityOptions(std::string_view email)
    : email(email) {
  CHECK(!email.empty());
}

AccountAvailabilityOptions::AccountAvailabilityOptions(
    std::string_view email,
    std::string_view gaia_id,
    std::optional<ConsentLevel> consent_level,
    std::optional<std::string> refresh_token,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    const std::vector<uint8_t>& wrapped_binding_key,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    raw_ptr<network::TestURLLoaderFactory> url_loader_factory_for_cookies,
    signin_metrics::AccessPoint access_point)
    : email(email),
      gaia_id(gaia_id),
      consent_level(consent_level),
      refresh_token(refresh_token),
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      wrapped_binding_key(wrapped_binding_key),
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      url_loader_factory_for_cookies(url_loader_factory_for_cookies),
      access_point(access_point) {
  CHECK(!email.empty());
}

AccountAvailabilityOptions::~AccountAvailabilityOptions() = default;

// --- AccountAvailabilityOptionsBuilder ---------------------------------------

AccountAvailabilityOptionsBuilder::AccountAvailabilityOptionsBuilder(
    network::TestURLLoaderFactory* url_loader_factory)
    : url_loader_factory_for_cookies_(url_loader_factory) {}

AccountAvailabilityOptionsBuilder::AccountAvailabilityOptionsBuilder(
    const AccountAvailabilityOptionsBuilder& other) = default;

AccountAvailabilityOptionsBuilder::AccountAvailabilityOptionsBuilder(
    AccountAvailabilityOptionsBuilder&& other) noexcept = default;

AccountAvailabilityOptionsBuilder& AccountAvailabilityOptionsBuilder::operator=(
    const AccountAvailabilityOptionsBuilder& other) = default;

AccountAvailabilityOptionsBuilder& AccountAvailabilityOptionsBuilder::operator=(
    AccountAvailabilityOptionsBuilder&& other) noexcept = default;

AccountAvailabilityOptionsBuilder::~AccountAvailabilityOptionsBuilder() =
    default;

AccountAvailabilityOptionsBuilder& AccountAvailabilityOptionsBuilder::AsPrimary(
    ConsentLevel consent_level) {
  primary_account_consent_level_ = consent_level;
  return *this;
}

AccountAvailabilityOptionsBuilder&
AccountAvailabilityOptionsBuilder::WithGaiaId(std::string_view gaia_id) {
  CHECK(!gaia_id.empty());
  gaia_id_ = gaia_id;
  return *this;
}

AccountAvailabilityOptionsBuilder&
AccountAvailabilityOptionsBuilder::WithCookie(bool with_cookie) {
  CHECK(!with_cookie || url_loader_factory_for_cookies_)
      << "URL loader factory must be non-null to set account cookies";
  with_cookie_ = with_cookie;
  return *this;
}

AccountAvailabilityOptionsBuilder&
AccountAvailabilityOptionsBuilder::WithRefreshToken(
    std::string_view refresh_token) {
  refresh_token_ = refresh_token;
  return *this;
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
AccountAvailabilityOptionsBuilder&
AccountAvailabilityOptionsBuilder::WithRefreshTokenBindingKey(
    const std::vector<uint8_t>& wrapped_binding_key) {
  CHECK(refresh_token_.has_value()) << "Binding key requires a refresh token";
  wrapped_binding_key_ = wrapped_binding_key;
  return *this;
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

AccountAvailabilityOptionsBuilder&
AccountAvailabilityOptionsBuilder::WithoutRefreshToken() {
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  CHECK(wrapped_binding_key_.empty()) << "Binding key requires a refresh token";
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  refresh_token_ = std::nullopt;
  return *this;
}

AccountAvailabilityOptionsBuilder&
AccountAvailabilityOptionsBuilder::WithAccessPoint(
    signin_metrics::AccessPoint access_point) {
  access_point_ = access_point;
  return *this;
}

AccountAvailabilityOptions AccountAvailabilityOptionsBuilder::Build(
    std::string_view email) {
  return AccountAvailabilityOptions(
      email, gaia_id_, primary_account_consent_level_, refresh_token_,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      wrapped_binding_key_,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      with_cookie_ ? url_loader_factory_for_cookies_ : nullptr, access_point_);
}

// -----------------------------------------------------------------------------

void WaitForRefreshTokensLoaded(IdentityManager* identity_manager) {
  base::RunLoop run_loop;
  TestIdentityManagerObserver load_credentials_observer(identity_manager);
  load_credentials_observer.SetOnRefreshTokensLoadedCallback(
      run_loop.QuitClosure());

  if (identity_manager->AreRefreshTokensLoaded())
    return;

  // Do NOT explicitly load credentials here:
  // 1. It is not re-entrant and will DCHECK fail.
  // 2. It should have been called by IdentityManager during its initialization.

  run_loop.Run();

  DCHECK(identity_manager->AreRefreshTokensLoaded());
}

std::optional<signin::ConsentLevel> GetPrimaryAccountConsentLevel(
    IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return std::nullopt;
  }

  // TODO(crbug.com/40067058): revisit this once `ConsentLevel::kSync` is
  // removed.
  return identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)
             ? signin::ConsentLevel::kSync
             : signin::ConsentLevel::kSignin;
}

CoreAccountInfo SetPrimaryAccount(IdentityManager* identity_manager,
                                  const std::string& email,
                                  ConsentLevel consent_level) {
  return MakeAccountAvailable(identity_manager,
                              AccountAvailabilityOptionsBuilder()
                                  .AsPrimary(consent_level)
                                  .WithoutRefreshToken()
                                  .Build(email));
}

void SetAutomaticIssueOfAccessTokens(IdentityManager* identity_manager,
                                     bool grant) {
  // Assumes that the given identity manager uses an underlying token service
  // of type FakeProfileOAuth2TokenService.
  CHECK(identity_manager->GetTokenService()
            ->IsFakeProfileOAuth2TokenServiceForTesting());
  static_cast<FakeProfileOAuth2TokenService*>(
      identity_manager->GetTokenService())
      ->set_auto_post_fetch_response_on_message_loop(grant);
}

void SetRefreshTokenForPrimaryAccount(IdentityManager* identity_manager,
                                      const std::string& token_value) {
  DCHECK(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin);
  SetRefreshTokenForAccount(identity_manager, account_id, token_value);
}

void SetInvalidRefreshTokenForPrimaryAccount(
    IdentityManager* identity_manager,
    signin_metrics::SourceForRefreshTokenOperation source) {
  DCHECK(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin);

  SetInvalidRefreshTokenForAccount(identity_manager, account_id, source);
}

void RemoveRefreshTokenForPrimaryAccount(IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount(ConsentLevel::kSignin))
    return;

  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin);

  RemoveRefreshTokenForAccount(identity_manager, account_id);
}

AccountInfo MakePrimaryAccountAvailable(IdentityManager* identity_manager,
                                        const std::string& email,
                                        ConsentLevel consent_level) {
  CoreAccountInfo account_info =
      MakeAccountAvailable(identity_manager, AccountAvailabilityOptionsBuilder()
                                                 .AsPrimary(consent_level)
                                                 .Build(email));
  AccountInfo primary_account_info =
      identity_manager->FindExtendedAccountInfo(account_info);
  // Ensure that extended information for the account is available after setting
  // the refresh token.
  CHECK(!primary_account_info.IsEmpty());
  return primary_account_info;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug.com/40067058): remove this function once `ConsentLevel::kSync` is
// removed.
void RevokeSyncConsent(IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount(ConsentLevel::kSync))
    return;

  DCHECK(identity_manager->GetPrimaryAccountMutator());
  base::RunLoop run_loop;
  TestIdentityManagerObserver signout_observer(identity_manager);
  signout_observer.SetOnPrimaryAccountChangedCallback(base::BindOnce(
      [](base::RunLoop* run_loop, PrimaryAccountChangeEvent event) {
        if (event.GetEventTypeFor(ConsentLevel::kSync) ==
            PrimaryAccountChangeEvent::Type::kCleared) {
          run_loop->Quit();
        }
      },
      &run_loop));
  identity_manager->GetPrimaryAccountMutator()->RevokeSyncConsent(
      signin_metrics::ProfileSignout::kTest);
  run_loop.Run();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void ClearPrimaryAccount(IdentityManager* identity_manager) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(blundell): If we ever need this functionality on ChromeOS (which seems
  // unlikely), plumb this through to just clear the primary account info
  // synchronously with IdentityManager.
  NOTREACHED_IN_MIGRATION();
#else
  if (!identity_manager->HasPrimaryAccount(ConsentLevel::kSignin))
    return;

  DCHECK(identity_manager->GetPrimaryAccountMutator());
  base::RunLoop run_loop;
  TestIdentityManagerObserver signout_observer(identity_manager);
  signout_observer.SetOnPrimaryAccountChangedCallback(base::BindOnce(
      [](base::RunLoop* run_loop, PrimaryAccountChangeEvent event) {
        if (event.GetEventTypeFor(ConsentLevel::kSignin) ==
            PrimaryAccountChangeEvent::Type::kCleared) {
          run_loop->Quit();
        }
      },
      &run_loop));
  identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kTest);

  run_loop.Run();
#endif
}

void WaitForPrimaryAccount(IdentityManager* identity_manager,
                           ConsentLevel consent_level,
                           const CoreAccountId& account_id) {
  if (identity_manager->GetPrimaryAccountId(consent_level) == account_id)
    return;

  base::RunLoop run_loop;
  TestIdentityManagerObserver primary_account_observer(identity_manager);
  primary_account_observer.SetOnPrimaryAccountChangedCallback(base::BindOnce(
      [](IdentityManager* identity_manager, ConsentLevel consent_level,
         const CoreAccountId& account_id, base::RunLoop* run_loop,
         PrimaryAccountChangeEvent event) {
        if (identity_manager->GetPrimaryAccountId(consent_level) == account_id)
          run_loop->Quit();
      },
      identity_manager, consent_level, account_id, &run_loop));
  run_loop.Run();
}

AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                 const AccountAvailabilityOptions& options) {
  if (options.refresh_token.has_value()) {
    // Wait until tokens are loaded, otherwise the account will be removed as
    // soon as tokens finish loading.
    WaitForRefreshTokensLoaded(identity_manager);
  }

  auto* account_tracker_service = identity_manager->GetAccountTrackerService();
  CHECK(account_tracker_service);

  if (options.consent_level.has_value()) {
    CoreAccountInfo primary_account_info =
        identity_manager->GetPrimaryAccountInfo(options.consent_level.value());
    CHECK(primary_account_info.IsEmpty() ||
          (primary_account_info.email != options.email &&
           options.consent_level.value() == ConsentLevel::kSignin));
  }

  if (account_tracker_service->FindAccountInfoByEmail(options.email)
          .IsEmpty()) {
    auto gaia = options.gaia_id.empty() ? GetTestGaiaIdForEmail(options.email)
                                        : options.gaia_id;
    account_tracker_service->SeedAccountInfo(gaia, options.email,
                                             options.access_point);
  }

  AccountInfo account_info =
      account_tracker_service->FindAccountInfoByEmail(options.email);
  CHECK(!account_info.account_id.empty());
  CHECK(options.gaia_id.empty() || account_info.gaia == options.gaia_id)
      << "The already available account does not match the requested gaia: '"
      << account_info.gaia << "' instead of '" << options.gaia_id << "'.";

  if (options.consent_level.has_value()) {
    auto consent_level = options.consent_level.value();
    PrimaryAccountManager* primary_account_manager =
        identity_manager->GetPrimaryAccountManager();
    primary_account_manager->SetPrimaryAccountInfo(account_info, consent_level,
                                                   options.access_point);
    CHECK_EQ(account_info.gaia,
             identity_manager->GetPrimaryAccountInfo(consent_level).gaia);
  }

  if (options.refresh_token.has_value()) {
    SetRefreshTokenForAccount(identity_manager, account_info.account_id,
                              options.refresh_token.value()
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                                  ,
                              options.wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    );
  }

  if (options.url_loader_factory_for_cookies) {
    AddCookieAccount(identity_manager, options.url_loader_factory_for_cookies,
                     {account_info.email, account_info.gaia});
  }

  return account_info;
}

AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                 const std::string& email) {
  return MakeAccountAvailable(identity_manager,
                              AccountAvailabilityOptionsBuilder().Build(email));
}

void SetRefreshTokenForAccount(IdentityManager* identity_manager,
                               const CoreAccountId& account_id,
                               const std::string& token_value
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                               ,
                               const std::vector<uint8_t>& wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
) {
  UpdateRefreshTokenForAccount(
      identity_manager->GetTokenService(),
      identity_manager->GetAccountTrackerService(), identity_manager,
      account_id,
      token_value.empty()
          ? "refresh_token_for_" + account_id.ToString() + "_" +
                base::Uuid::GenerateRandomV4().AsLowercaseString()
          : token_value
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  );
}

void SetInvalidRefreshTokenForAccount(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    signin_metrics::SourceForRefreshTokenOperation source) {
  UpdateRefreshTokenForAccount(identity_manager->GetTokenService(),
                               identity_manager->GetAccountTrackerService(),
                               identity_manager, account_id,
                               GaiaConstants::kInvalidRefreshToken,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                               /*wrapped_binding_key=*/{},
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                               source);
}

void RemoveRefreshTokenForAccount(IdentityManager* identity_manager,
                                  const CoreAccountId& account_id) {
  if (!identity_manager->HasAccountWithRefreshToken(account_id))
    return;

  base::RunLoop run_loop;
  TestIdentityManagerObserver token_updated_observer(identity_manager);
  token_updated_observer.SetOnRefreshTokenRemovedCallback(
      run_loop.QuitClosure());

  // TODO(crbug.com/40776160): simplify this when all Lacros Profiles use
  // Mirror.
#if BUILDFLAG(IS_CHROMEOS)
  if (ShouldUseAccountManagerFacade(identity_manager)) {
    const AccountInfo& account_info =
        identity_manager->GetAccountTrackerService()->GetAccountInfo(
            account_id);
    GetAccountManagerFacade(identity_manager)
        ->RemoveAccountForTesting(account_manager::AccountKey{
            account_info.gaia, account_manager::AccountType::kGaia});
  } else
#endif  // BUILDFLAG(IS_CHROMEOS)
  {
    identity_manager->GetTokenService()->RevokeCredentials(account_id);
  }

  run_loop.Run();
}

void AddCookieAccount(IdentityManager* identity_manager,
                      network::TestURLLoaderFactory* test_url_loader_factory,
                      const CookieParamsForTest& cookie_account_to_add) {
  signin::AccountsInCookieJarInfo cookie_info =
      identity_manager->GetAccountsInCookieJar();

  std::vector<CookieParamsForTest> gaia_cookie_accounts;
  for (const gaia::ListedAccount& existing_cookie_account :
       cookie_info.GetPotentiallyInvalidSignedInAccounts()) {
    if (existing_cookie_account.email == cookie_account_to_add.email &&
        existing_cookie_account.gaia_id == cookie_account_to_add.gaia_id) {
      // No need to add the account, a matching one is already present. Abort.
      return;
    }

    gaia_cookie_accounts.push_back(
        {existing_cookie_account.email, existing_cookie_account.gaia_id});
  }
  gaia_cookie_accounts.push_back(cookie_account_to_add);
  SetCookieAccounts(identity_manager, test_url_loader_factory,
                    gaia_cookie_accounts);
}

void SetCookieAccounts(
    IdentityManager* identity_manager,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::vector<CookieParamsForTest>& cookie_accounts) {
  // Convert |cookie_accounts| to the format list_accounts_test_utils wants.
  std::vector<CookieParams> gaia_cookie_accounts;
  for (const CookieParamsForTest& params : cookie_accounts) {
    gaia_cookie_accounts.push_back({params.email, params.gaia_id,
                                    /*valid=*/true, params.signed_out,
                                    /*verified=*/true});
  }

  base::RunLoop run_loop;
  TestIdentityManagerObserver cookie_observer(identity_manager);
  cookie_observer.SetOnAccountsInCookieUpdatedCallback(run_loop.QuitClosure());

  SetListAccountsResponseWithParams(gaia_cookie_accounts,
                                    test_url_loader_factory);

  GaiaCookieManagerService* cookie_manager =
      identity_manager->GetGaiaCookieManagerService();
  cookie_manager->set_list_accounts_stale_for_testing(true);
  // Clears cached LIST_ACCOUNTS requests, so that the new request can trigger
  // the observers instead of being assumed as having an identical result as the
  // previous one.
  // TODO(crbug.com/40273636): Investigate replacing this by
  // `cookie_manager->ForceOnCookieChangeProcessing()`.
  cookie_manager->CancelAll();
  cookie_manager->ListAccounts();

  run_loop.Run();
}

void TriggerListAccount(
    IdentityManager* identity_manager,
    network::TestURLLoaderFactory* test_url_loader_factory) {
  const AccountsInCookieJarInfo& cookie_jar =
      identity_manager->GetAccountsInCookieJar();
  // Construct the cookie params with the actual cookies in the cookie jar.
  std::vector<CookieParamsForTest> cookie_params;
  for (auto& account : cookie_jar.GetPotentiallyInvalidSignedInAccounts()) {
    cookie_params.emplace_back(account.email, account.gaia_id,
                               /*signed_out=*/false);
  }
  for (auto& account : cookie_jar.GetSignedOutAccounts()) {
    cookie_params.emplace_back(account.email, account.gaia_id,
                               /*signed_out=*/true);
  }

  // Trigger the /ListAccount with the current cookie information.
  SetCookieAccounts(identity_manager, test_url_loader_factory, cookie_params);
}

AccountInfo WithGeneratedUserInfo(const AccountInfo& base_account_info,
                                  std::string_view given_name) {
  CHECK(!given_name.empty())
      << "A given name is needed to generate the Gaia info.";

  AccountInfo extended_account_info = base_account_info;

  extended_account_info.given_name = given_name;
  extended_account_info.full_name = base::StrCat({given_name, " FullName"});

  extended_account_info.picture_url =
      "https://chromium.org/examples/account_picture.jpg";
  extended_account_info.hosted_domain = kNoHostedDomainFound;
  extended_account_info.locale = "en";

  CHECK(extended_account_info.IsValid());

  return extended_account_info;
}

void UpdateAccountInfoForAccount(IdentityManager* identity_manager,
                                 AccountInfo account_info) {
  // Make sure the account being updated is a known account.

  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();

  DCHECK(account_tracker_service);
  DCHECK(!account_tracker_service->GetAccountInfo(account_info.account_id)
              .account_id.empty());

  account_tracker_service->SeedAccountInfo(account_info);
}

void SimulateAccountImageFetch(IdentityManager* identity_manager,
                               const CoreAccountId& account_id,
                               const std::string& image_url_with_size,
                               const gfx::Image& image) {
  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();
  account_tracker_service->SetAccountImage(account_id, image_url_with_size,
                                           image);
}

void SetFreshnessOfAccountsInGaiaCookie(IdentityManager* identity_manager,
                                        bool accounts_are_fresh) {
  GaiaCookieManagerService* cookie_manager =
      identity_manager->GetGaiaCookieManagerService();
  cookie_manager->set_list_accounts_stale_for_testing(!accounts_are_fresh);
}

std::string GetTestGaiaIdForEmail(const std::string& email) {
  std::string gaia_id =
      std::string("gaia_id_for_") + gaia::CanonicalizeEmail(email);
  // Avoid character '@' in the gaia ID string as there is code in the codebase
  // that asserts that a gaia ID does not contain a "@" character.
  std::replace(gaia_id.begin(), gaia_id.end(), '@', '_');
  return gaia_id;
}

void UpdatePersistentErrorOfRefreshTokenForAccount(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error) {
  DCHECK(identity_manager->HasAccountWithRefreshToken(account_id));
  identity_manager->GetTokenService()->GetDelegate()->UpdateAuthError(
      account_id, auth_error);
}

void WaitForErrorStateOfRefreshTokenUpdatedForAccount(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    base::RepeatingCallback<bool(const GoogleServiceAuthError&)> predicate) {
  DCHECK(identity_manager->HasAccountWithRefreshToken(account_id));

  base::RunLoop run_loop;
  base::RepeatingClosure check_error_status =
      base::BindRepeating(&CompareErrorStatusAndCallClosure, identity_manager,
                          account_id, predicate, run_loop.QuitClosure());
  TestIdentityManagerObserver token_updated_observer(identity_manager);
  token_updated_observer.SetOnErrorStateOfRefreshTokenUpdatedCallback(
      check_error_status);
  // Call callback explicitly to check the current error state before waiting.
  check_error_status.Run();

  run_loop.Run();
}

void DisableAccessTokenFetchRetries(IdentityManager* identity_manager) {
  identity_manager->GetTokenService()
      ->set_max_authorization_token_fetch_retries_for_testing(0);
}

#if BUILDFLAG(IS_ANDROID)
void SetUpMockAccountManagerFacade(bool useFakeImpl) {
  Java_AccountManagerFacadeUtil_setUpMockFacade(
      base::android::AttachCurrentThread(), useFakeImpl);
}
#endif

void CancelAllOngoingGaiaCookieOperations(IdentityManager* identity_manager) {
  identity_manager->GetGaiaCookieManagerService()->CancelAll();
}

void SimulateSuccessfulFetchOfAccountInfo(IdentityManager* identity_manager,
                                          const CoreAccountId& account_id,
                                          const std::string& email,
                                          const std::string& gaia,
                                          const std::string& hosted_domain,
                                          const std::string& full_name,
                                          const std::string& given_name,
                                          const std::string& locale,
                                          const std::string& picture_url) {
  base::Value::Dict user_info;
  user_info.Set("id", gaia);
  user_info.Set("email", email);
  user_info.Set("hd", hosted_domain);
  user_info.Set("name", full_name);
  user_info.Set("given_name", given_name);
  user_info.Set("locale", locale);
  user_info.Set("picture", picture_url);

  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();
  account_tracker_service->SetAccountInfoFromUserInfo(account_id, user_info);
}

#if BUILDFLAG(IS_CHROMEOS)
account_manager::AccountManagerFacade* GetAccountManagerFacade(
    IdentityManager* identity_manager) {
  return identity_manager->GetAccountManagerFacade();
}
#endif

}  // namespace signin
