// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_TEST_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_TEST_SIGNIN_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/account_manager_core/account.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif

class PrefService;

// An implementation of SigninClient for use in unittests. Instantiates test
// versions of the various objects that SigninClient is required to provide as
// part of its interface.
class TestSigninClient : public SigninClient {
 public:
  TestSigninClient(
      PrefService* pref_service,
      network::TestURLLoaderFactory* test_url_loader_factory = nullptr);

  TestSigninClient(const TestSigninClient&) = delete;
  TestSigninClient& operator=(const TestSigninClient&) = delete;

  ~TestSigninClient() override;

  // SigninClient implementation that is specialized for unit tests.

  void DoFinalInit() override;

  // Returns nullptr.
  // NOTE: This should be changed to return a properly-initalized PrefService
  // once there is a unit test that requires it.
  PrefService* GetPrefs() override;

  // Returns true if clear primary account is allowed regardless of the consent
  // level.
  bool IsClearPrimaryAccountAllowed() const override;

  // Allow or disallow continuation of sign-out depending on value of
  // |is_clear_primary_account_allowed_|;
  void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric) override;

  // Wraps the test_url_loader_factory().
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

  network::mojom::CookieManager* GetCookieManager() override;
  void set_cookie_manager(
      std::unique_ptr<network::mojom::CookieManager> cookie_manager) {
    cookie_manager_ = std::move(cookie_manager);
  }

  // Returns |test_url_loader_factory_| if it is specified. Otherwise, lazily
  // creates a default factory and returns it.
  network::TestURLLoaderFactory* GetTestURLLoaderFactory();

  // Pass a TestURLLoader factory to use instead of the default one.
  void OverrideTestUrlLoaderFactory(network::TestURLLoaderFactory* factory);

  void set_are_signin_cookies_allowed(bool value) {
    are_signin_cookies_allowed_ = value;
  }

  void set_is_clear_primary_account_allowed(SignoutDecision value) {
    is_clear_primary_account_allowed_ = value;
  }

  // When |value| is true, network calls posted through DelayNetworkCall() are
  // delayed indefinitely.
  // When |value| is false, all pending calls are unblocked, and new calls are
  // executed immediately.
  void SetNetworkCallsDelayed(bool value);

  // SigninClient overrides:
  bool AreSigninCookiesAllowed() override;
  bool AreSigninCookiesDeletedOnExit() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  void DelayNetworkCall(base::OnceClosure callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) override;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  absl::optional<account_manager::Account> GetInitialPrimaryAccount() override;
  absl::optional<bool> IsInitialPrimaryAccountChild() const override;

  void SetInitialPrimaryAccountForTests(const account_manager::Account& account,
                                        const absl::optional<bool>& is_child);
  void RemoveAccount(const account_manager::AccountKey& account_key) override;

  void RemoveAllAccounts() override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
  std::unique_ptr<network::TestURLLoaderFactory>
      default_test_url_loader_factory_;
  raw_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;

  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<network::mojom::CookieManager> cookie_manager_;
  bool are_signin_cookies_allowed_;
  bool network_calls_delayed_;
  SignoutDecision is_clear_primary_account_allowed_ = SignoutDecision::ALLOW;

  std::vector<base::OnceClosure> delayed_network_calls_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  absl::optional<account_manager::Account> initial_primary_account_;
  absl::optional<bool> is_initial_primary_account_child_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_TEST_SIGNIN_CLIENT_H_
