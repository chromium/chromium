// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_parts_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_impl.h"
#include "chromeos/ash/components/osauth/impl/auth_session_storage_impl.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"

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
std::unique_ptr<AuthParts> AuthParts::Create() {
  std::unique_ptr<AuthPartsImpl> result = std::make_unique<AuthPartsImpl>();
  result->CreateDefaultComponents();
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

void AuthPartsImpl::CreateDefaultComponents() {
  session_storage_ =
      std::make_unique<AuthSessionStorageImpl>(UserDataAuthClient::Get());
  auth_hub_ = std::make_unique<AuthHubImpl>();
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

void AuthPartsImpl::RegisterEngineFactory(
    std::unique_ptr<AuthFactorEngineFactory> factory) {
  engine_factories_.push_back(std::move(factory));
}

const std::vector<std::unique_ptr<AuthFactorEngineFactory>>&
AuthPartsImpl::GetEngineFactories() {
  return engine_factories_;
}

}  // namespace ash
