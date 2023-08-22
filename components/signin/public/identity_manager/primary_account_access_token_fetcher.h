// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_ACCESS_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/core_account_id.h"

class GoogleServiceAuthError;

namespace signin {
struct AccessTokenInfo;

// Class that supports obtaining OAuth2 access tokens for the user's primary
// account.See ./README.md for the definition of "accounts with OAuth2 refresh
// tokens" and "primary account".
//
// The usage model of this class is as follows: When a
// PrimaryAccountAccessTokenFetcher is created, it will make an access token
// request for the primary account (either immediately or if/once the primary
// account becomes available, based on the value of the specified |Mode|
// parameter). When the access token request is fulfilled the
// PrimaryAccountAccessTokenFetcher will call the specified callback, at which
// point it is safe for the caller to destroy the object. If the object is
// destroyed before the request is fulfilled the request is dropped and the
// callback will never be invoked. This class may only be used on the UI thread.
//
// To drive responses to access token fetches in unittests of clients of this
// class, use IdentityTestEnvironment.
//
// Concrete usage example (related concrete test example follows):
//   class MyClass {
//    public:
//     MyClass(IdentityManager* identity_manager) :
//       identity_manager_(identity_manager) {
//         // An access token request could also be initiated at any arbitrary
//         // point in the lifetime of |MyClass|.
//         StartAccessTokenRequestForPrimaryAccount();
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
//     std::unique_ptr<PrimaryAccountAccessTokenFetcher> access_token_fetcher_;
//     std::string access_token_;
//     GoogleServiceAuthError access_token_request_error_;
//
//     // Most commonly invoked as part of some larger flow to hit a Gaia
//     // endpoint for a client-specific purpose (e.g., hitting sync
//     // endpoints).
//     // Could also be public, but in general, any clients that would need to
//     // create access token requests could and should just create
//     // PrimaryAccountAccessTokenFetchers directly themselves rather than
//     // introducing wrapper API surfaces.
//
//     MyClass::StartAccessTokenRequestForPrimaryAccount() {
//       // Choose scopes to obtain for the access token.
//       ScopeSet scopes;
//       scopes.insert(GaiaConstants::kMyFirstScope);
//       scopes.insert(GaiaConstants::kMySecondScope);
//
//       // Choose the mode in which to fetch the access token:
//       // see AccessTokenFetcher::Mode below for definitions.
//       auto mode =
//         PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable;
//
//       // Create the fetcher.
//       access_token_fetcher_ =
//           std::make_unique<PrimaryAccountAccessTokenFetcher>(
//               /*consumer_name=*/"MyClass",
//               identity_manager_,
//               scopes,
//               mode,
//               base::BindOnce(&MyClass::OnAccessTokenRequestCompleted,
//                              // It is safe to use base::Unretained as
//                              // |this| owns |access_token_fetcher_|.
//                              base::Unretained(this)));
//
//     }
//     void MyClass::OnAccessTokenRequestCompleted(
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
// Concrete test example:
//   TEST(MyClassTest, SuccessfulAccessTokenFetchForPrimaryAccount) {
//     IdentityTestEnvironment identity_test_env;
//
//
//     MyClass my_class(identity_test_env.identity_manager());
//
//     // Make the primary account available, which should result in an access
//     // token fetch being made on behalf of |my_class|.
//     identity_test_env.
//         MakePrimaryAccountAvailable("test_email", ConsentLevel::kSignin);
//
//     identity_test_env.
//         WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
//             "access_token", base::Time::Max());
//
//     // MyClass::OnAccessTokenRequestCompleted() will have been invoked with
//     // an AccessTokenInfo object containing the above-specified parameters;
//     // the test can now perform any desired validation of expected actions
//     // |MyClass| took in response.
//   }
//
// It is also possible to create a PrimaryAccountAccessTokenFetcher that does
// not immediately start its access token request, as follows:
//
//       auto access_token_fetcher =
//           std::make_unique<PrimaryAccountAccessTokenFetcher>(
//               /*consumer_name=*/"MyClass",
//               identity_manager_,
//               scopes,
//               mode);
//
// This allows an ownership model wherein MyClass does not store a
// std::unique_ptr<PrimaryAccountAccessTokenFetcher> instance variable; instead,
// the fetcher is bound to the |TokenCallback| itself and destroyed
// automatically when the callback is called or destroyed. Continuing the
// example from above:
//
//       auto* access_token_fetcher_ptr = access_token_fetcher.get();
//       access_token_fetcher_ptr.Start(
//           base::BindOnce(&MyClass::OnAccessTokenRequestCompleted,
//                          // |this| no longer owns |access_token_fetcher|, so
//                          // a |WeakPtr| is required.
//                          my_class_weak_factory_.GetWeakPtr()),
//                          std::move(access_token_fetcher));
//
//     void MyClass::OnAccessTokenRequestCompleted(
//         std::unique_ptr<PrimaryAccountAccessTokenFetcher> fetcher,
//         GoogleServiceAuthError error,
//         AccessTokenInfo access_token_info) {
//       ...
//       // |fetcher| is destroyed automatically when this callback finishes.
//     }
class PrimaryAccountAccessTokenFetcher : public IdentityManager::Observer {
 public:
  // Specifies how this instance should behave:
  // |kImmediate|: Makes one-shot immediate request.
  // |kWaitUntilAvailable|: Waits for the primary account to be available
  // before making the request. In particular, "available" is defined as the
  // moment when (a) there is a primary account and (b) that account has a
  // refresh token. This semantics is richer than using an AccessTokenFetcher in
  // kWaitUntilRefreshTokenAvailable mode, as the latter will make a request
  // once the specified account has a refresh token, regardless of whether it's
  // the primary account at that point.
  // Note that using |kWaitUntilAvailable| can result in waiting forever
  // if the user is not signed in and doesn't sign in.
  enum class Mode { kImmediate, kWaitUntilAvailable };

  // Instantiates an OAuth2 access token fetcher for the given |scopes|. Once
  // the fetcher is created, the caller initiates its token request using
  // |Start()|. See ./README.md.
  PrimaryAccountAccessTokenFetcher(const std::string& oauth_consumer_name,
                                   IdentityManager* identity_manager,
                                   const ScopeSet& scopes,
                                   Mode mode,
                                   ConsentLevel consent);

  // Convenience constructor that immediately issues the access token request.
  PrimaryAccountAccessTokenFetcher(const std::string& oauth_consumer_name,
                                   IdentityManager* identity_manager,
                                   const ScopeSet& scopes,
                                   AccessTokenFetcher::TokenCallback callback,
                                   Mode mode,
                                   ConsentLevel consent);

  PrimaryAccountAccessTokenFetcher(const PrimaryAccountAccessTokenFetcher&) =
      delete;
  PrimaryAccountAccessTokenFetcher& operator=(
      const PrimaryAccountAccessTokenFetcher&) = delete;

  ~PrimaryAccountAccessTokenFetcher() override;

  // Starts the process of obtaining an OAuth2 access token for the fetcher's
  // |scopes_|. |callback| is called once the request completes (successfully
  // or not). If the fetcher is destroyed before the process completes, the
  // callback is not called.
  void Start(AccessTokenFetcher::TokenCallback callback);

  // Exposed for tests.
  bool access_token_request_retried() { return access_token_retried_; }

 private:
  // Returns the primary account ID. If consent is |kSignin| this may be
  // the "unconsented" primary account ID.
  CoreAccountId GetAccountId() const;

  // Returns true iff there is a primary account with a refresh token. Should
  // only be called in mode |kWaitUntilAvailable|.
  bool AreCredentialsAvailable() const;

  void StartAccessTokenRequest();

  // IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(const PrimaryAccountChangeEvent& event) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  // IdentityManager::DiagnosticsObserver implementation.
  void OnIdentityManagerShutdown(IdentityManager* identity_manager) override;

  // Checks whether credentials are now available and starts an access token
  // request if so. Should only be called in mode |kWaitUntilAvailable|.
  void ProcessSigninStateChange();

  // Invoked by |fetcher_| when an access token request completes.
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  AccessTokenInfo access_token_info);

  std::string oauth_consumer_name_;
  raw_ptr<IdentityManager, DanglingUntriaged> identity_manager_;
  ScopeSet scopes_;

  // Per the contract of this class, it is allowed for clients to delete this
  // object as part of the invocation of |callback_|. Hence, this object must
  // assume that it is dead after invoking |callback_| and must not run any more
  // code.
  AccessTokenFetcher::TokenCallback callback_;

  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};

  // Internal fetcher that does the actual access token request.
  std::unique_ptr<AccessTokenFetcher> access_token_fetcher_;

  // When a token request gets canceled, we want to retry once.
  bool access_token_retried_ = false;

  // Used in kWaitUntilAvailable mode when waiting for the account to be
  // available.
  bool waiting_for_account_available_ = false;

  Mode mode_;

  const ConsentLevel consent_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_ACCESS_TOKEN_FETCHER_H_
