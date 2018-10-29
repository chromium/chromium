// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_TEST_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_TEST_SIGNIN_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/signin/core/browser/signin_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_url_loader_factory.h"

class PrefService;

// An implementation of SigninClient for use in unittests. Instantiates test
// versions of the various objects that SigninClient is required to provide as
// part of its interface.
class TestSigninClient : public SigninClient {
 public:
  TestSigninClient(PrefService* pref_service);
  ~TestSigninClient() override;

  // SigninClient implementation that is specialized for unit tests.

  void DoFinalInit() override;

  // Returns NULL.
  // NOTE: This should be changed to return a properly-initalized PrefService
  // once there is a unit test that requires it.
  PrefService* GetPrefs() override;

  // Does nothing.
  void OnSignedOut() override;

  // Trace that this was called.
  void PostSignedIn(const std::string& account_id,
                    const std::string& username,
                    const std::string& password) override;

  // Allow or disallow continuation of sign-out depending on value of
  // |is_signout_allowed_|;
  void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric) override;

  std::string get_signed_in_password() { return signed_in_password_; }

  // Returns the empty string.
  std::string GetProductVersion() override;

  // Wraps the test_url_loader_factory().
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

  // Returns FakeCookieManager.
  network::mojom::CookieManager* GetCookieManager() override;

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  void set_are_signin_cookies_allowed(bool value) {
    are_signin_cookies_allowed_ = value;
  }

  void set_is_signout_allowed(bool value) { is_signout_allowed_ = value; }

  // When |value| is true, network calls posted through DelayNetworkCall() are
  // delayed indefinitely.
  // When |value| is false, all pending calls are unblocked, and new calls are
  // executed immediately.
  void SetNetworkCallsDelayed(bool value);

  // SigninClient overrides:
  bool IsFirstRun() const override;
  base::Time GetInstallDate() override;
  bool AreSigninCookiesAllowed() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  void DelayNetworkCall(const base::Closure& callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;
  void PreGaiaLogout(base::OnceClosure callback) override;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  PrefService* pref_service_;
  std::unique_ptr<network::mojom::CookieManager> cookie_manager_;
  bool are_signin_cookies_allowed_;
  bool network_calls_delayed_;
  bool is_signout_allowed_;
  std::vector<base::OnceClosure> delayed_network_calls_;

  // Pointer to be filled by PostSignedIn.
  std::string signed_in_password_;

  DISALLOW_COPY_AND_ASSIGN(TestSigninClient);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_TEST_SIGNIN_CLIENT_H_
