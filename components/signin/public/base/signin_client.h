// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_CLIENT_H_

#include <memory>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "url/gurl.h"

class PrefService;

namespace content_settings {
class Observer;
}

namespace network {
class SharedURLLoaderFactory;

namespace mojom {
class CookieManager;
}
}  // namespace network

// An interface that needs to be supplied to the Signin component by its
// embedder.
class SigninClient : public KeyedService {
 public:
  // Argument to PreSignOut() callback, indicating client decision.
  enum class SignoutDecision { ALLOW_SIGNOUT, DISALLOW_SIGNOUT };

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

  // Called before Google sign-out started. Implementers must run the
  // |on_signout_decision_reached|, passing a SignoutDecision to allow/disallow
  // sign-out to continue. When to disallow sign-out is implementation specific.
  // Sign-out is always allowed by default.
  virtual void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric);

  // Called before calling the GAIA logout endpoint.
  // For iOS, cookies should be cleaned up.
  virtual void PreGaiaLogout(base::OnceClosure callback);

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

  // Execute |callback| if and when there is a network connection.
  virtual void DelayNetworkCall(base::OnceClosure callback) = 0;

  // Creates a new platform-specific GaiaAuthFetcher.
  virtual std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) = 0;

  // Marks the DICE migration completed.
  virtual void SetDiceMigrationCompleted() {}

  // Schedules migration to happen at next startup.
  virtual void SetReadyForDiceMigration(bool is_ready) {}

  // Checks whether a user is known to be non-enterprise. Domains such as
  // gmail.com and googlemail.com are known to not be managed. Also returns
  // false if the username is empty.
  virtual bool IsNonEnterpriseUser(const std::string& username);
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_CLIENT_H_
