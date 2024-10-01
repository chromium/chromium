// Copyright 2012 The Chromium Authors
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

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"

namespace signin {
class AccountsInCookieJarInfo;
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
                             signin::IdentityManager::DiagnosticsObserver,
                             AccountReconcilor::Observer {
 public:
  class Observer {
   public:
    // |info| will contain the dictionary of signin_status_ values as indicated
    // in the comments for GetSigninStatus() below.
    virtual void OnSigninStateChanged(const base::Value::Dict& info) = 0;

    // Notification that the cookie accounts are ready to be displayed.
    virtual void OnCookieAccountsFetched(const base::Value::Dict& info) = 0;
  };

  AboutSigninInternals(signin::IdentityManager* identity_manager,
                       SigninErrorController* signin_error_controller,
                       signin::AccountConsistencyMethod account_consistency,
                       SigninClient* client,
                       AccountReconcilor* account_reconcilor);

  AboutSigninInternals(const AboutSigninInternals&) = delete;
  AboutSigninInternals& operator=(const AboutSigninInternals&) = delete;

  ~AboutSigninInternals() override;

  // Registers the preferences used by AboutSigninInternals.
  static void RegisterPrefs(PrefRegistrySimple* user_prefs);

  // Each instance of SigninInternalsUI adds itself as an observer to be
  // notified of all updates that AboutSigninInternals receives.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Pulls all signin values that have been persisted in the user prefs.
  void RefreshSigninPrefs();

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
  base::Value::Dict GetSigninStatus();

  // signin::IdentityManager::Observer implementations.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

 private:
  // Encapsulates diagnostic information about tokens for different services.
  struct TokenInfo {
    TokenInfo(const std::string& consumer_id, const signin::ScopeSet& scopes);
    ~TokenInfo();
    base::Value::Dict ToValue() const;

    static bool LessThan(const std::unique_ptr<TokenInfo>& a,
                         const std::unique_ptr<TokenInfo>& b);

    // Called when the token is invalidated.
    void Invalidate();

    std::string consumer_id;    // service that requested the token.
    signin::ScopeSet scopes;    // Scoped that are requested.
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
                         const signin::ScopeSet& scopes);

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
    base::Value::Dict ToValue(
        signin::IdentityManager* identity_manager,
        SigninErrorController* signin_error_controller,
        SigninClient* signin_client,
        signin::AccountConsistencyMethod account_consistency,
        AccountReconcilor* account_reconcilor);
  };

  // IdentityManager::DiagnosticsObserver implementations.
  void OnAccessTokenRequested(const CoreAccountId& account_id,
                              const std::string& consumer_id,
                              const signin::ScopeSet& scopes) override;
  void OnAccessTokenRequestCompleted(const CoreAccountId& account_id,
                                     const std::string& consumer_id,
                                     const signin::ScopeSet& scopes,
                                     const GoogleServiceAuthError& error,
                                     base::Time expiration_time) override;
  void OnAccessTokenRemovedFromCache(const CoreAccountId& account_id,
                                     const signin::ScopeSet& scopes) override;
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
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  // Notifies that the value of `field` is changed. This function will update
  // the corresponding field in `signin_status_` and the underlying prefs.
  //
  // If `value` is empty, then this function will clear the prefs and reset
  // the corresponding entry in `signin_status_`.
  void NotifyTimedSigninFieldValueChanged(
      const signin_internals_util::TimedSigninStatusField& field,
      const std::string& value);

  void NotifyObservers();

  // SigninErrorController::Observer implementation
  void OnErrorChanged() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // AccountReconcilor::Observer implementation.
  void OnBlockReconcile() override;
  void OnUnblockReconcile() override;
  void OnStateChanged(signin_metrics::AccountReconcilorState state) override;

  // Weak pointer to the identity manager.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // Weak pointer to the client.
  raw_ptr<SigninClient> client_;

  // Weak pointer to the SigninErrorController
  raw_ptr<SigninErrorController> signin_error_controller_;

  // Weak pointer to the AccountReconcilor.
  raw_ptr<AccountReconcilor, DanglingUntriaged> account_reconcilor_;

  // Encapsulates the actual signin and token related values.
  // Most of the values are mirrored in the prefs for persistence.
  SigninStatus signin_status_;

  signin::AccountConsistencyMethod account_consistency_;

  base::ObserverList<Observer>::Unchecked signin_observers_;

  // Used to keep track of observerations.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observeration_{this};

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::DiagnosticsObserver>
      diganostics_observeration_{this};

  base::ScopedObservation<SigninClient, content_settings::Observer>
      client_observeration_{this};

  base::ScopedObservation<SigninErrorController,
                          SigninErrorController::Observer>
      signin_error_observeration_{this};

  base::ScopedObservation<AccountReconcilor, AccountReconcilor::Observer>
      account_reconcilor_observeration_{this};
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_
