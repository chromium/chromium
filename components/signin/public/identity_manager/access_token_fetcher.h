// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace network {
class SharedURLLoaderFactory;
}

class GoogleServiceAuthError;

namespace signin {
struct AccessTokenInfo;

// Class that supports obtaining OAuth2 access tokens for any of the user's
// accounts with OAuth2 refresh tokens. Note that in the common case of
// obtaining an OAuth2 access token for the user's primary account, use
// PrimaryAccountAccessTokenFetcher rather than this class. See ./README.md
// for the definition of "accounts with OAuth2 refresh tokens" and "primary
// account".
//
// The usage model of this class is as follows: When an AccessTokenFetcher is
// created via IdentityManager::CreateAccessTokenFetcherXXX(), the returned
// object is owned by the caller. This object will make at most one access
// token request for the specified account (either immediately or if/once a
// refresh token for the specified account becomes available, based on the
// value of the specified |Mode| parameter). When the access token request is
// fulfilled the AccessTokenFetcher will call the specified callback, at which
// point it is safe for the caller to destroy the object. If the object is
// destroyed before the request is fulfilled the request is dropped and the
// callback will never be invoked.  This class may only be used on the UI
// thread.
//
// To drive responses to access token fetches in unittests of clients of this
// class, use IdentityTestEnvironment.
//
// Concrete usage example (related concrete test example follows):
//   class MyClass {
//    public:
//     MyClass(IdentityManager* identity_manager, account_id) :
//       identity_manager_(identity_manager) {
//         // An access token request could also be initiated at any arbitrary
//         // point in the lifetime of |MyClass|.
//         StartAccessTokenRequestForAccount(account_id);
//       }
//
//
//     ~MyClass() {
//       // If the access token request is still live, the destruction of
//       |access_token_fetcher_| will cause it to be dropped.
//     }
//
//    private:
//     IdentityManager* identity_manager_;
//     std::unique_ptr<AccessTokenFetcher> access_token_fetcher_;
//     std::string access_token_;
//     GoogleServiceAuthError access_token_request_error_;
//
//     // Most commonly invoked as part of some larger flow to hit a Gaia
//     // endpoint for a client-specific purpose (e.g., hitting sync
//     // endpoints).
//     // Could also be public, but in general, any clients that would need to
//     // create access token requests could and should just create
//     // AccessTokenFetchers directly themselves rather than introducing
//     // wrapper API surfaces.
//     MyClass::StartAccessTokenRequestForAccount(CoreAccountId account_id) {
//       // Choose scopes to obtain for the access token.
//       ScopeSet scopes;
//       scopes.insert(GaiaConstants::kMyFirstScope);
//       scopes.insert(GaiaConstants::kMySecondScope);

//       // Choose the mode in which to fetch the access token:
//       // see AccessTokenFetcher::Mode below for definitions.
//       auto mode = signin::AccessTokenFetcher::Mode::kImmediate;

//       // Create the fetcher via |identity_manager_|.
//       access_token_fetcher_ =
//           identity_manager_->CreateAccessTokenFetcherForAccount(
//               account_id, /*consumer_name=*/"MyClass",
//               scopes,
//               base::BindOnce(&MyClass::OnAccessTokenRequestCompleted,
//                              // It is safe to use base::Unretained as
//                              // |this| owns |access_token_fetcher_|.
//                              base::Unretained(this)),
//                              mode);
//
//     }
//     MyClass::OnAccessTokenRequestCompleted(
//         GoogleServiceAuthError error, AccessTokenInfo access_token_info) {
//       // It is safe to destroy |access_token_fetcher_| from this callback.
//       access_token_fetcher_.reset();
//
//       if (error.state() == GoogleServiceAuthError::NONE) {
//         // The fetcher successfully obtained an access token.
//         access_token_ = access_token_info.token;
//         // MyClass can now take whatever action required having an access
//         // token (e.g.,hitting a given Gaia endpoint).
//         ...
//       } else {
//         // The fetcher failed to obtain a token; |error| specifies why.
//         access_token_request_error_ = error;
//         // MyClass can now perform any desired error handling.
//         ...
//       }
//     }
//   }
//
//   Concrete test example:
//   TEST(MyClassTest, SuccessfulAccessTokenFetch) {
//     IdentityTestEnvironment identity_test_env;
//     AccountInfo account_info =
//         identity_test_env.MakeAccountAvailable("test_email");
//
//     MyClass my_class(identity_test_env.identity_manager(),
//                      account_info.account_id);
//     identity_test_env.
//         WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
//             "access_token", base::Time::Max());
//
//     // MyClass::OnAccessTokenRequestCompleted() will have been invoked with
//     // an AccessTokenInfo object containing the above-specified parameters;
//     // the test can now perform any desired validation of expected actions
//     // |MyClass| took in response.
//   }
class AccessTokenFetcher : public ProfileOAuth2TokenServiceObserver,
                           public OAuth2AccessTokenManager::Consumer {
 public:
  // Specifies how this instance should behave:
  // |kImmediate|: Makes one-shot immediate request.
  // |kWaitUntilRefreshTokenAvailable|: Waits for the account to have a refresh
  // token before making the request.
  // Note that using |kWaitUntilRefreshTokenAvailable| can result in waiting
  // forever if the user is not signed in and doesn't sign in.
  enum class Mode { kImmediate, kWaitUntilRefreshTokenAvailable };

  // Callback for when a request completes (successful or not). On successful
  // requests, |error| is NONE and |access_token_info| contains info of the
  // obtained OAuth2 access token. On failed requests, |error| contains the
  // actual error and |access_token_info| is empty.
  // NOTE: At the time that this method is invoked, it is safe for the client to
  // destroy the AccessTokenFetcher instance that is invoking this callback.
  using TokenCallback =
      base::OnceCallback<void(GoogleServiceAuthError error,
                              AccessTokenInfo access_token_info)>;

  // Instantiates a fetcher and immediately starts the process of obtaining an
  // OAuth2 access token for |account_id| and |scopes|. The |callback| is called
  // once the request completes (successful or not). If the AccessTokenFetcher
  // is destroyed before the process completes, the callback is not called.
  AccessTokenFetcher(const CoreAccountId& account_id,
                     const std::string& oauth_consumer_name,
                     ProfileOAuth2TokenService* token_service,
                     PrimaryAccountManager* primary_account_manager,
                     const ScopeSet& scopes,
                     TokenCallback callback,
                     Mode mode,
                     bool require_sync_consent_for_scope_verification);

  // Instantiates a fetcher and immediately starts the process of obtaining an
  // OAuth2 access token for |account_id| and |scopes|, allowing clients to pass
  // a |url_loader_factory| of their choice. The |callback| is called
  // once the request completes (successful or not). If the AccessTokenFetcher
  // is destroyed before the process completes, the callback is not called.
  AccessTokenFetcher(
      const CoreAccountId& account_id,
      const std::string& oauth_consumer_name,
      ProfileOAuth2TokenService* token_service,
      PrimaryAccountManager* primary_account_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const ScopeSet& scopes,
      TokenCallback callback,
      Mode mode,
      bool require_sync_consent_for_scope_verification);

  AccessTokenFetcher(const AccessTokenFetcher&) = delete;
  AccessTokenFetcher& operator=(const AccessTokenFetcher&) = delete;

  ~AccessTokenFetcher() override;

 private:
  // Returns true iff a refresh token is available for |account_id_|. Should
  // only be called in mode |kWaitUntilAvailable|.
  bool IsRefreshTokenAvailable() const;

  // Verifies that the client has the appropriate level of user consent for all
  // of the requested scopes.
  void VerifyScopeAccess();

  void StartAccessTokenRequest();

  // ProfileOAuth2TokenServiceObserver implementation.
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override;

  // OAuth2AccessTokenManager::Consumer implementation.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Invokes |callback_| with (|error|, |access_token_info|). Per the contract
  // of this class, it is allowed for clients to delete this object as part of
  // the invocation of |callback_|. Hence, this object must assume that it is
  // dead after invoking this method and must not run any more code.
  void RunCallbackAndMaybeDie(GoogleServiceAuthError error,
                              AccessTokenInfo access_token_info);

  const CoreAccountId account_id_;
  raw_ptr<ProfileOAuth2TokenService, DanglingUntriaged> token_service_;
  // Suppress unused typedef warnings in some compiler builds when DCHECK is
  // disabled.
  [[maybe_unused]] raw_ptr<PrimaryAccountManager, DanglingUntriaged>
      primary_account_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const ScopeSet scopes_;
  // NOTE: This callback should only be invoked from |RunCallbackAndMaybeDie|,
  // as invoking it has the potential to destroy this object per this class's
  // contract.
  TokenCallback callback_;
  const Mode mode_;

  // TODO(crbug.com/40067025): Remove this field once
  // kReplaceSyncPromosWithSignInPromos launches.
  const bool require_sync_consent_for_scope_verification_;

  base::ScopedObservation<ProfileOAuth2TokenService,
                          ProfileOAuth2TokenServiceObserver>
      token_service_observation_{this};

  std::unique_ptr<OAuth2AccessTokenManager::Request> access_token_request_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_FETCHER_H_
