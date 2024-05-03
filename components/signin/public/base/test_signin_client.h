// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_TEST_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_TEST_SIGNIN_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/wait_for_network_callback_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include <optional>

#include "components/account_manager_core/account.h"
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "components/signin/public/base/bound_session_oauth_multilogin_delegate.h"
#endif

class PrefService;

namespace version_info {
enum class Channel;
}

class TestWaitForNetworkCallbackHelper : public WaitForNetworkCallbackHelper {
 public:
  TestWaitForNetworkCallbackHelper();
  ~TestWaitForNetworkCallbackHelper() override;

  // WaitForNetworkCallbackHelper:
  bool AreNetworkCallsDelayed() override;
  void DelayNetworkCall(base::OnceClosure callback) override;

  void SetNetworkCallsDelayed(bool delayed);

 private:
  bool network_calls_delayed_ = false;
  std::vector<base::OnceClosure> delayed_network_calls_;
};

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

  // Wraps the test_url_loader_factory().
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

  network::mojom::CookieManager* GetCookieManager() override;
  void set_cookie_manager(
      std::unique_ptr<network::mojom::CookieManager> cookie_manager) {
    cookie_manager_ = std::move(cookie_manager);
  }

  network::mojom::NetworkContext* GetNetworkContext() override;

  // Returns |test_url_loader_factory_| if it is specified. Otherwise, lazily
  // creates a default factory and returns it.
  network::TestURLLoaderFactory* GetTestURLLoaderFactory();

  // Pass a TestURLLoader factory to use instead of the default one.
  void OverrideTestUrlLoaderFactory(network::TestURLLoaderFactory* factory);

  void set_are_signin_cookies_allowed(bool value) {
    are_signin_cookies_allowed_ = value;
  }

  void set_are_signin_cookies_deleted_on_exit(bool value) {
    are_signin_cookies_deleted_on_exit_ = value;
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
  bool AreNetworkCallsDelayed() override;
  void DelayNetworkCall(base::OnceClosure callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) override;
  version_info::Channel GetClientChannel() override;
  void OnPrimaryAccountChanged(
      signin::PrimaryAccountChangeEvent event_details) override;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  std::unique_ptr<signin::BoundSessionOAuthMultiLoginDelegate>
  CreateBoundSessionOAuthMultiloginDelegate() const override;

  void SetBoundSessionOauthMultiloginDelegateFactory(
      base::RepeatingCallback<
          std::unique_ptr<signin::BoundSessionOAuthMultiLoginDelegate>()>
          factory);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<account_manager::Account> GetInitialPrimaryAccount() override;
  std::optional<bool> IsInitialPrimaryAccountChild() const override;

  void SetInitialPrimaryAccountForTests(const account_manager::Account& account,
                                        const std::optional<bool>& is_child);
  void RemoveAccount(const account_manager::AccountKey& account_key) override;

  void RemoveAllAccounts() override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  using BoundSessionOauthMultiloginDelegateFactory = base::RepeatingCallback<
      std::unique_ptr<signin::BoundSessionOAuthMultiLoginDelegate>()>;
#endif  //  BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  std::unique_ptr<TestWaitForNetworkCallbackHelper>
      test_wait_for_network_callback_helper_;
  std::unique_ptr<network::TestURLLoaderFactory>
      default_test_url_loader_factory_;
  raw_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;

  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<network::mojom::CookieManager> cookie_manager_;
  std::unique_ptr<network::mojom::NetworkContext> network_context_;
  bool are_signin_cookies_allowed_;
  bool are_signin_cookies_deleted_on_exit_ = false;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  BoundSessionOauthMultiloginDelegateFactory bound_session_delegate_factory_;
#endif  //  BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<account_manager::Account> initial_primary_account_;
  std::optional<bool> is_initial_primary_account_child_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_TEST_SIGNIN_CLIENT_H_
