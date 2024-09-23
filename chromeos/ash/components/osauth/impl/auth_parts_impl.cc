// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_parts_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_impl.h"
#include "chromeos/ash/components/osauth/impl/auth_session_storage_impl.h"
#include "chromeos/ash/components/osauth/impl/cryptohome_core_impl.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_password_engine.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_pin_engine.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_smart_card_engine.h"
#include "chromeos/ash/components/osauth/impl/engines/prefs_pin_engine.h"
#include "chromeos/ash/components/osauth/impl/login_screen_auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

AuthPartsImpl* g_instance = nullptr;

}

// static
std::unique_ptr<AuthPartsImpl> AuthPartsImpl::CreateTestInstance() {
  std::unique_ptr<AuthPartsImpl> result = std::make_unique<AuthPartsImpl>();
  return result;
}

// static
std::unique_ptr<AuthParts> AuthParts::Create(PrefService* local_state) {
  std::unique_ptr<AuthPartsImpl> result = std::make_unique<AuthPartsImpl>();
  result->CreateDefaultComponents(local_state);
  return result;
}

// static
AuthParts* AuthParts::Get() {
  CHECK(g_instance);
  return g_instance;
}

AuthPartsImpl::AuthPartsImpl() {
  CHECK(!g_instance);
  g_instance = this;
}

AuthPartsImpl::~AuthPartsImpl() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void AuthPartsImpl::CreateDefaultComponents(PrefService* local_state) {
  session_storage_ = std::make_unique<AuthSessionStorageImpl>(
      UserDataAuthClient::Get(), base::DefaultClock::GetInstance());
  factors_cache_ = std::make_unique<AuthFactorPresenceCache>(local_state);
  auth_hub_ = std::make_unique<AuthHubImpl>(factors_cache_.get());
  cryptohome_core_ =
      std::make_unique<CryptohomeCoreImpl>(UserDataAuthClient::Get());
  RegisterEngineFactory(std::make_unique<CryptohomePasswordEngineFactory>());

  if (!features::IsUseAuthPanelInSessionEnabled()) {
    RegisterEngineFactory(
        std::make_unique<CryptohomePinEngineFactory>(local_state));
    RegisterEngineFactory(std::make_unique<CryptohomeSmartCardEngineFactory>());
    RegisterEngineFactory(
        std::make_unique<PrefsPinEngineFactory>(*local_state));
  }

  login_screen_policy_connector_ =
      std::make_unique<LoginScreenAuthPolicyConnector>(local_state);
  legacy_auth_surface_registry_ = std::make_unique<LegacyAuthSurfaceRegistry>();
  auth_surface_registry_ = std::make_unique<AuthSurfaceRegistry>();
}

AuthSessionStorage* AuthPartsImpl::GetAuthSessionStorage() {
  CHECK(session_storage_);
  return session_storage_.get();
}

AuthHub* AuthPartsImpl::GetAuthHub() {
  CHECK(auth_hub_);
  return auth_hub_.get();
}

void AuthPartsImpl::SetAuthHub(std::unique_ptr<AuthHub> auth_hub) {
  CHECK(!auth_hub_);
  auth_hub_ = std::move(auth_hub);
}

void AuthPartsImpl::SetAuthSessionStorage(
    std::unique_ptr<AuthSessionStorage> auth_session_storage) {
  CHECK(!session_storage_);
  session_storage_ = std::move(auth_session_storage);
}

CryptohomeCore* AuthPartsImpl::GetCryptohomeCore() {
  CHECK(cryptohome_core_);
  return cryptohome_core_.get();
}

void AuthPartsImpl::RegisterEngineFactory(
    std::unique_ptr<AuthFactorEngineFactory> factory) {
  engine_factories_.push_back(std::move(factory));
}

const std::vector<std::unique_ptr<AuthFactorEngineFactory>>&
AuthPartsImpl::GetEngineFactories() {
  return engine_factories_;
}

void AuthPartsImpl::RegisterEarlyLoginAuthPolicyConnector(
    std::unique_ptr<AuthPolicyConnector> connector) {
  CHECK(!early_login_policy_connector_);
  early_login_policy_connector_ = std::move(connector);
  early_login_policy_connector_->SetLoginScreenAuthPolicyConnector(
      login_screen_policy_connector_.get());
}

void AuthPartsImpl::ReleaseEarlyLoginAuthPolicyConnector() {
  early_login_policy_connector_.reset();
}

void AuthPartsImpl::SetProfilePrefsAuthPolicyConnector(
    AuthPolicyConnector* connector) {
  if (profile_prefs_policy_connector_) {
    CHECK_IS_TEST();
    LOG(WARNING) << "Overriding ProfilePrefsAuthPolicyConnector in test";
    profile_prefs_policy_connector_->OnShutdown();
  }

  profile_prefs_policy_connector_ = connector;
  profile_prefs_policy_connector_->SetLoginScreenAuthPolicyConnector(
      login_screen_policy_connector_.get());
}

AuthPolicyConnector* AuthPartsImpl::GetAuthPolicyConnector() {
  if (profile_prefs_policy_connector_) {
    return profile_prefs_policy_connector_;
  }
  if (early_login_policy_connector_) {
    return early_login_policy_connector_.get();
  }
  CHECK(login_screen_policy_connector_);
  return login_screen_policy_connector_.get();
}

void AuthPartsImpl::Shutdown() {
  if (auth_hub_) {
    auth_hub_->Shutdown();
  }
  if (profile_prefs_policy_connector_) {
    profile_prefs_policy_connector_->OnShutdown();
  }
}

LegacyAuthSurfaceRegistry* AuthPartsImpl::GetLegacyAuthSurfaceRegistry() {
  return legacy_auth_surface_registry_.get();
}

AuthSurfaceRegistry* AuthPartsImpl::GetAuthSurfaceRegistry() {
  return auth_surface_registry_.get();
}

}  // namespace ash
