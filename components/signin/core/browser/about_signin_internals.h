// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/oauth2_token_service.h"

class AccountTrackerService;
class PrefRegistrySimple;
class ProfileOAuth2TokenService;
class SigninClient;

// Many values in SigninStatus are also associated with a timestamp.
// This makes it easier to keep values and their associated times together.
using TimedSigninStatusValue = std::pair<std::string, std::string>;

// This class collects authentication, signin and token information
// to propagate to about:signin-internals via SigninInternalsUI.
class AboutSigninInternals
    : public KeyedService,
      public signin_internals_util::SigninDiagnosticsObserver,
      public OAuth2TokenService::Observer,
      public OAuth2TokenService::DiagnosticsObserver,
      public GaiaCookieManagerService::Observer,
      SigninManagerBase::Observer,
      SigninErrorController::Observer {
 public:
  class Observer {
   public:
    // |info| will contain the dictionary of signin_status_ values as indicated
    // in the comments for GetSigninStatus() below.
    virtual void OnSigninStateChanged(const base::DictionaryValue* info) = 0;

    // Notification that the cookie accounts are ready to be displayed.
    virtual void OnCookieAccountsFetched(const base::DictionaryValue* info) = 0;
  };

  AboutSigninInternals(ProfileOAuth2TokenService* token_service,
                       AccountTrackerService* account_tracker,
                       SigninManagerBase* signin_manager,
                       SigninErrorController* signin_error_controller,
                       GaiaCookieManagerService* cookie_manager_service,
                       signin::AccountConsistencyMethod account_consistency);
  ~AboutSigninInternals() override;

  // Registers the preferences used by AboutSigninInternals.
  static void RegisterPrefs(PrefRegistrySimple* user_prefs);

  // Each instance of SigninInternalsUI adds itself as an observer to be
  // notified of all updates that AboutSigninInternals receives.
  void AddSigninObserver(Observer* observer);
  void RemoveSigninObserver(Observer* observer);

  // Pulls all signin values that have been persisted in the user prefs.
  void RefreshSigninPrefs();

  void Initialize(SigninClient* client);

  void OnRefreshTokenReceived(const std::string& status);
  void OnAuthenticationResultReceived(const std::string& status);

  // KeyedService implementation.
  void Shutdown() override;

  // Returns a dictionary of values in signin_status_ for use in
  // about:signin-internals. The values are formatted as shown -
  //
  // { "signin_info" :
  //     [ {"title": "Basic Information",
  //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
  //       },
  //       { "title": "Detailed Information",
  //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
  //       }],
  //   "token_info" :
  //     [ List of {"name": "foo-name", "token" : "foo-token",
  //                 "status": "foo_stat", "time" : "foo_time"} elems]
  //  }
  std::unique_ptr<base::DictionaryValue> GetSigninStatus();

  // GaiaCookieManagerService::Observer implementations.
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const std::vector<gaia::ListedAccount>& signed_out_accounts,
      const GoogleServiceAuthError& error) override;

 private:
  // Encapsulates diagnostic information about tokens for different services.
  struct TokenInfo {
    TokenInfo(const std::string& consumer_id,
              const OAuth2TokenService::ScopeSet& scopes);
    ~TokenInfo();
    std::unique_ptr<base::DictionaryValue> ToValue() const;

    static bool LessThan(const std::unique_ptr<TokenInfo>& a,
                         const std::unique_ptr<TokenInfo>& b);

    // Called when the token is invalidated.
    void Invalidate();

    std::string consumer_id;              // service that requested the token.
    OAuth2TokenService::ScopeSet scopes;  // Scoped that are requested.
    base::Time request_time;
    base::Time receive_time;
    base::Time expiration_time;
    GoogleServiceAuthError error;
    bool removed_;
  };

  enum class RefreshTokenEventType {
    kUpdateToRegular,
    kUpdateToInvalid,
    kRevokeRegular,
    kAllTokensLoaded,
  };

  struct RefreshTokenEvent {
    RefreshTokenEvent();
    std::string GetTypeAsString() const;

    const base::Time timestamp;
    std::string account_id;
    RefreshTokenEventType type;
    std::string source;
  };

  // Encapsulates both authentication and token related information. Used
  // by SigninInternals to maintain information that needs to be shown in
  // the about:signin-internals page.
  struct SigninStatus {
    std::vector<TimedSigninStatusValue> timed_signin_fields;

    // Map account id to tokens associated to the account.
    std::map<std::string, std::vector<std::unique_ptr<TokenInfo>>>
        token_info_map;

    // All the events that affected the refresh tokens.
    std::deque<RefreshTokenEvent> refresh_token_events;

    SigninStatus();
    ~SigninStatus();

    TokenInfo* FindToken(const std::string& account_id,
                         const std::string& consumer_id,
                         const OAuth2TokenService::ScopeSet& scopes);

    void AddRefreshTokenEvent(const RefreshTokenEvent& event);

    // Returns a dictionary with the following form:
    // { "signin_info" :
    //     [ {"title": "Basic Information",
    //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
    //       },
    //       { "title": "Detailed Information",
    //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
    //       }],
    //   "token_info" :
    //     [ List of
    //       { "title": account id,
    //         "data": [List of {"service" : service name,
    //                           "scopes" : requested scoped,
    //                           "request_time" : request time,
    //                           "status" : request status} elems]
    //       }],
    //  }
    std::unique_ptr<base::DictionaryValue> ToValue(
        AccountTrackerService* account_tracker,
        SigninManagerBase* signin_manager,
        SigninErrorController* signin_error_controller,
        ProfileOAuth2TokenService* token_service,
        GaiaCookieManagerService* cookie_manager_service_,
        SigninClient* signin_client,
        signin::AccountConsistencyMethod account_consistency);
  };

  // SigninManager::SigninDiagnosticsObserver implementation.
  void NotifySigninValueChanged(
      const signin_internals_util::TimedSigninStatusField& field,
      const std::string& value) override;

  // OAuth2TokenService::DiagnosticsObserver implementations.
  void OnAccessTokenRequested(
      const std::string& account_id,
      const std::string& consumer_id,
      const OAuth2TokenService::ScopeSet& scopes) override;
  void OnFetchAccessTokenComplete(const std::string& account_id,
                                  const std::string& consumer_id,
                                  const OAuth2TokenService::ScopeSet& scopes,
                                  GoogleServiceAuthError error,
                                  base::Time expiration_time) override;
  void OnAccessTokenRemoved(
      const std::string& account_id,
      const OAuth2TokenService::ScopeSet& scopes) override;

  // OAuth2TokenServiceDelegate::Observer implementations.
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnEndBatchChanges() override;

  // SigninManagerBase::Observer implementations.
  void GoogleSigninFailed(const GoogleServiceAuthError& error) override;
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username) override;
  void GoogleSignedOut(const std::string& account_id,
                               const std::string& username) override;

  void NotifyObservers();

  // SigninErrorController::Observer implementation
  void OnErrorChanged() override;

  // Weak pointer to the token service.
  ProfileOAuth2TokenService* token_service_;

  // Weak pointer to the account tracker.
  AccountTrackerService* account_tracker_;

  // Weak pointer to the signin manager.
  SigninManagerBase* signin_manager_;

  // Weak pointer to the client.
  SigninClient* client_;

  // Weak pointer to the SigninErrorController
  SigninErrorController* signin_error_controller_;

  // Weak pointer to the GaiaCookieManagerService
  GaiaCookieManagerService* cookie_manager_service_;

  // Encapsulates the actual signin and token related values.
  // Most of the values are mirrored in the prefs for persistence.
  SigninStatus signin_status_;

  signin::AccountConsistencyMethod account_consistency_;

  base::ObserverList<Observer>::Unchecked signin_observers_;

  DISALLOW_COPY_AND_ASSIGN(AboutSigninInternals);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_
