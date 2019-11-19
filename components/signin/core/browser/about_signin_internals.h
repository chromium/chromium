// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/identity/public/cpp/scope_set.h"

namespace signin {
struct AccountsInCookieJarInfo;
}

class PrefRegistrySimple;
class SigninClient;

// Many values in SigninStatus are also associated with a timestamp.
// This makes it easier to keep values and their associated times together.
using TimedSigninStatusValue = std::pair<std::string, std::string>;

// This class collects authentication, signin and token information
// to propagate to about:signin-internals via SigninInternalsUI.
class AboutSigninInternals : public KeyedService,
                             public content_settings::Observer,
                             SigninErrorController::Observer,
                             signin::IdentityManager::Observer,
                             signin::IdentityManager::DiagnosticsObserver {
 public:
  class Observer {
   public:
    // |info| will contain the dictionary of signin_status_ values as indicated
    // in the comments for GetSigninStatus() below.
    virtual void OnSigninStateChanged(const base::DictionaryValue* info) = 0;

    // Notification that the cookie accounts are ready to be displayed.
    virtual void OnCookieAccountsFetched(const base::DictionaryValue* info) = 0;
  };

  AboutSigninInternals(signin::IdentityManager* identity_manager,
                       SigninErrorController* signin_error_controller,
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

  // signin::IdentityManager::Observer implementations.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

 private:
  // Encapsulates diagnostic information about tokens for different services.
  struct TokenInfo {
    TokenInfo(const std::string& consumer_id, const identity::ScopeSet& scopes);
    ~TokenInfo();
    std::unique_ptr<base::DictionaryValue> ToValue() const;

    static bool LessThan(const std::unique_ptr<TokenInfo>& a,
                         const std::unique_ptr<TokenInfo>& b);

    // Called when the token is invalidated.
    void Invalidate();

    std::string consumer_id;    // service that requested the token.
    identity::ScopeSet scopes;  // Scoped that are requested.
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
    CoreAccountId account_id;
    RefreshTokenEventType type;
    std::string source;
  };

  // Encapsulates both authentication and token related information. Used
  // by SigninInternals to maintain information that needs to be shown in
  // the about:signin-internals page.
  struct SigninStatus {
    std::vector<TimedSigninStatusValue> timed_signin_fields;

    // Map account id to tokens associated to the account.
    std::map<CoreAccountId, std::vector<std::unique_ptr<TokenInfo>>>
        token_info_map;

    // All the events that affected the refresh tokens.
    std::deque<RefreshTokenEvent> refresh_token_events;

    SigninStatus();
    ~SigninStatus();

    TokenInfo* FindToken(const CoreAccountId& account_id,
                         const std::string& consumer_id,
                         const identity::ScopeSet& scopes);

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
        signin::IdentityManager* identity_manager,
        SigninErrorController* signin_error_controller,
        SigninClient* signin_client,
        signin::AccountConsistencyMethod account_consistency);
  };

  // IdentityManager::DiagnosticsObserver implementations.
  void OnAccessTokenRequested(const CoreAccountId& account_id,
                              const std::string& consumer_id,
                              const identity::ScopeSet& scopes) override;
  void OnAccessTokenRequestCompleted(const CoreAccountId& account_id,
                                     const std::string& consumer_id,
                                     const identity::ScopeSet& scopes,
                                     GoogleServiceAuthError error,
                                     base::Time expiration_time) override;
  void OnAccessTokenRemovedFromCache(const CoreAccountId& account_id,
                                     const identity::ScopeSet& scopes) override;
  void OnRefreshTokenUpdatedForAccountFromSource(
      const CoreAccountId& account_id,
      bool is_refresh_token_valid,
      const std::string& source) override;
  void OnRefreshTokenRemovedForAccountFromSource(
      const CoreAccountId& account_id,
      const std::string& source) override;

  // IdentityManager::Observer implementations.
  void OnRefreshTokensLoaded() override;
  void OnEndBatchOfRefreshTokenStateChanges() override;
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& primary_account_info) override;

  void NotifyTimedSigninFieldValueChanged(
      const signin_internals_util::TimedSigninStatusField& field,
      const std::string& value);

  void NotifyObservers();

  // SigninErrorController::Observer implementation
  void OnErrorChanged() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  // Weak pointer to the identity manager.
  signin::IdentityManager* identity_manager_;

  // Weak pointer to the client.
  SigninClient* client_;

  // Weak pointer to the SigninErrorController
  SigninErrorController* signin_error_controller_;

  // Encapsulates the actual signin and token related values.
  // Most of the values are mirrored in the prefs for persistence.
  SigninStatus signin_status_;

  signin::AccountConsistencyMethod account_consistency_;

  base::ObserverList<Observer>::Unchecked signin_observers_;

  DISALLOW_COPY_AND_ASSIGN(AboutSigninInternals);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_
