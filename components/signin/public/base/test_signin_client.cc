// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/test_signin_client.h"

#include <memory>

#include "base/check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

TestSigninClient::TestSigninClient(
    PrefService* pref_service,
    network::TestURLLoaderFactory* test_url_loader_factory)
    : test_url_loader_factory_(test_url_loader_factory),
      pref_service_(pref_service),
      are_signin_cookies_allowed_(true),
      network_calls_delayed_(false),
      is_signout_allowed_(true),
      is_dice_migration_completed_(false) {}

TestSigninClient::~TestSigninClient() {}

void TestSigninClient::DoFinalInit() {}

PrefService* TestSigninClient::GetPrefs() {
  return pref_service_;
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
  return GetTestURLLoaderFactory()->GetSafeWeakWrapper();
}

network::mojom::CookieManager* TestSigninClient::GetCookieManager() {
  if (!cookie_manager_)
    cookie_manager_ = std::make_unique<network::TestCookieManager>();
  return cookie_manager_.get();
}

network::TestURLLoaderFactory* TestSigninClient::GetTestURLLoaderFactory() {
  if (test_url_loader_factory_)
    return test_url_loader_factory_;

  if (!default_test_url_loader_factory_) {
    default_test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
  }

  return default_test_url_loader_factory_.get();
}

void TestSigninClient::OverrideTestUrlLoaderFactory(
    network::TestURLLoaderFactory* factory) {
  DCHECK(!default_test_url_loader_factory_);
  DCHECK(!test_url_loader_factory_);
  test_url_loader_factory_ = factory;
}

void TestSigninClient::SetNetworkCallsDelayed(bool value) {
  network_calls_delayed_ = value;

  if (!network_calls_delayed_) {
    for (base::OnceClosure& call : delayed_network_calls_)
      std::move(call).Run();
    delayed_network_calls_.clear();
  }
}

bool TestSigninClient::AreSigninCookiesAllowed() {
  return are_signin_cookies_allowed_;
}

bool TestSigninClient::AreSigninCookiesDeletedOnExit() {
  return false;
}

void TestSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {}

void TestSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {}

void TestSigninClient::DelayNetworkCall(base::OnceClosure callback) {
  if (network_calls_delayed_) {
    delayed_network_calls_.push_back(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

std::unique_ptr<GaiaAuthFetcher> TestSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source) {
  return std::make_unique<GaiaAuthFetcher>(consumer, source,
                                           GetURLLoaderFactory());
}

void TestSigninClient::SetDiceMigrationCompleted() {
  is_dice_migration_completed_ = true;
}

bool TestSigninClient::IsNonEnterpriseUser(const std::string& email) {
  return gaia::ExtractDomainName(email) == "gmail.com";
}
