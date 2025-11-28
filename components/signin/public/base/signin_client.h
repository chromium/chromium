// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_CLIENT_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/scoped_observation_traits.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/base/oauth_consumer_registry.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"

namespace signin {
class BoundSessionOAuthMultiLoginDelegate;
class PrimaryAccountChangeEvent;
}

class GaiaAuthConsumer;
class GaiaAuthFetcher;
class PrefService;

namespace content_settings {
class Observer;
}

namespace gaia {
class GaiaSource;
}

namespace network {
class SharedURLLoaderFactory;

namespace mojom {
class CookieManager;
class DeviceBoundSessionManager;
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace version_info {
enum class Channel;
}

// An interface that needs to be supplied to the Signin component by its
// embedder.
class SigninClient : public KeyedService {
 public:
  // Argument to PreSignOut() callback, indicating client decision.
  enum class SignoutDecision {
    ALLOW,
    CLEAR_PRIMARY_ACCOUNT_DISALLOWED,
    // Revoke sync disallowed implies that removing the primary account is also
    // disallowed since sync is attached to the primary account.
    REVOKE_SYNC_DISALLOWED,
  };

  ~SigninClient() override = default;

  // Call when done local initialization and SigninClient can initiate any work
  // it has to do that may require other components (like ProfileManager) to be
  // available.
  virtual void DoFinalInit() = 0;

  // Gets the preferences associated with the client.
  virtual PrefService* GetPrefs() = 0;

  // Returns the SharedURLLoaderFactory that should be used to fetch resources
  // associated with the client.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Returns the CookieManager for the client.
  virtual network::mojom::CookieManager* GetCookieManager() = 0;

  // Returns the DeviceBoundSessionManager for the client.
  //
  // TODO(crbug.com/463979316): Make it pure virtual to make sure all embedders
  // explicitly provide an implementation.
  virtual network::mojom::DeviceBoundSessionManager*
  GetDeviceBoundSessionManager() const;

  // Returns the NetworkContext for the client.
  virtual network::mojom::NetworkContext* GetNetworkContext() = 0;

  // Returns true if clearing the primary account is allowed regardless of the
  // consent level.
  virtual bool IsClearPrimaryAccountAllowed() const;
  virtual bool IsRevokeSyncConsentAllowed() const;

  bool is_clear_primary_account_allowed_for_testing() const;

  void set_is_clear_primary_account_allowed_for_testing(SignoutDecision value) {
    is_clear_primary_account_allowed_for_testing_ = value;
  }

  // Called before Google sign-out started. Implementers must run the
  // |on_signout_decision_reached|, passing a SignoutDecision to allow/disallow
  // sign-out to continue. When to disallow sign-out is implementation specific.
  // Sign-out is always allowed by default.
  virtual void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric);

  // Returns true if GAIA cookies are allowed in the content area.
  virtual bool AreSigninCookiesAllowed() = 0;

  // Returns true if signin cookies are cleared on exit.
  virtual bool AreSigninCookiesDeletedOnExit() = 0;

  // Adds an observer to listen for changes to the state of sign in cookie
  // settings.
  virtual void AddContentSettingsObserver(
      content_settings::Observer* observer) = 0;
  virtual void RemoveContentSettingsObserver(
      content_settings::Observer* observer) = 0;

  // Returns `true` if network calls will be delayed by `DelayNetworkCall()`.
  virtual bool AreNetworkCallsDelayed() = 0;

  // Execute `callback` if and when there is a network connection. Also see
  // `AreNetworkCallsDelayed()`.
  virtual void DelayNetworkCall(base::OnceClosure callback) = 0;

  // Creates a new platform-specific GaiaAuthFetcher.
  virtual std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) = 0;

  // Returns the channel for the client installation.
  virtual version_info::Channel GetClientChannel() = 0;

  // Called when the primary account is changed. `event_details` contains
  // information on how the account changed, and on how the signin/signout
  // happened.
  virtual void OnPrimaryAccountChanged(
      signin::PrimaryAccountChangeEvent event_details) = 0;

  virtual std::unique_ptr<signin::BoundSessionOAuthMultiLoginDelegate>
  CreateBoundSessionOAuthMultiloginDelegate() const;

  // Returns the OAuthConsumer associated with `oauth_consumer_id`.
  virtual signin::OAuthConsumer GetOAuthConsumerFromId(
      signin::OAuthConsumerId oauth_consumer_id) const = 0;

 protected:
  std::optional<SignoutDecision> is_clear_primary_account_allowed_for_testing_;
};

namespace base {

template <>
struct ScopedObservationTraits<SigninClient, content_settings::Observer> {
  static void AddObserver(SigninClient* source,
                          content_settings::Observer* observer) {
    source->AddContentSettingsObserver(observer);
  }
  static void RemoveObserver(SigninClient* source,
                             content_settings::Observer* observer) {
    source->RemoveContentSettingsObserver(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_CLIENT_H_
