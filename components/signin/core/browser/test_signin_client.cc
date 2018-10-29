// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/test_signin_client.h"

#include <memory>

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class FakeCookieManager : public network::mojom::CookieManager {
 public:
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          bool secure_source,
                          bool modify_http_only,
                          SetCanonicalCookieCallback callback) override;
  void GetAllCookies(GetAllCookiesCallback callback) override {}
  void GetCookieList(const GURL& url,
                     const net::CookieOptions& cookie_options,
                     GetCookieListCallback callback) override {}
  void DeleteCanonicalCookie(const net::CanonicalCookie& cookie,
                             DeleteCanonicalCookieCallback callback) override {}
  void DeleteCookies(network::mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override {}
  void AddCookieChangeListener(
      const GURL& url,
      const std::string& name,
      network::mojom::CookieChangeListenerPtr listener) override {}
  void AddGlobalChangeListener(
      network::mojom::CookieChangeListenerPtr notification_pointer) override {}
  void CloneInterface(
      network::mojom::CookieManagerRequest new_interface) override {}
  void FlushCookieStore(FlushCookieStoreCallback callback) override {}
  void SetContentSettings(
      const std::vector<::ContentSettingPatternSource>& settings) override {}
  void SetForceKeepSessionState() override {}
  void BlockThirdPartyCookies(bool block) override {}
};
}  // namespace

TestSigninClient::TestSigninClient(PrefService* pref_service)
    : shared_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      pref_service_(pref_service),
      are_signin_cookies_allowed_(true),
      network_calls_delayed_(false),
      is_signout_allowed_(true) {}

TestSigninClient::~TestSigninClient() {}

void TestSigninClient::DoFinalInit() {}

PrefService* TestSigninClient::GetPrefs() {
  return pref_service_;
}

void FakeCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    bool secure_source,
    bool modify_http_only,
    SetCanonicalCookieCallback callback) {
  std::move(callback).Run(false);
}

void TestSigninClient::OnSignedOut() {}

void TestSigninClient::PostSignedIn(const std::string& account_id,
                  const std::string& username,
                  const std::string& password) {
  signed_in_password_ = password;
}

void TestSigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric) {
  std::move(on_signout_decision_reached)
      .Run(is_signout_allowed_ ? SignoutDecision::ALLOW_SIGNOUT
                               : SignoutDecision::DISALLOW_SIGNOUT);
}

scoped_refptr<network::SharedURLLoaderFactory>
TestSigninClient::GetURLLoaderFactory() {
  return shared_factory_;
}

network::mojom::CookieManager* TestSigninClient::GetCookieManager() {
  if (!cookie_manager_)
    cookie_manager_ = std::make_unique<FakeCookieManager>();
  return cookie_manager_.get();
}

std::string TestSigninClient::GetProductVersion() { return ""; }

void TestSigninClient::SetNetworkCallsDelayed(bool value) {
  network_calls_delayed_ = value;

  if (!network_calls_delayed_) {
    for (base::OnceClosure& call : delayed_network_calls_)
      std::move(call).Run();
    delayed_network_calls_.clear();
  }
}

bool TestSigninClient::IsFirstRun() const {
  return false;
}

base::Time TestSigninClient::GetInstallDate() {
  return base::Time::Now();
}

bool TestSigninClient::AreSigninCookiesAllowed() {
  return are_signin_cookies_allowed_;
}

void TestSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
}

void TestSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
}

void TestSigninClient::DelayNetworkCall(const base::Closure& callback) {
  if (network_calls_delayed_) {
    delayed_network_calls_.push_back(callback);
  } else {
    callback.Run();
  }
}

std::unique_ptr<GaiaAuthFetcher> TestSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    const std::string& source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<GaiaAuthFetcher>(consumer, source,
                                           url_loader_factory);
}

void TestSigninClient::PreGaiaLogout(base::OnceClosure callback) {
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}
