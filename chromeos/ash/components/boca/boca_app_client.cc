// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_app_client.h"

#include "base/check_op.h"

namespace ash::boca {

namespace {

inline constexpr char kDummyDeviceId[] = "kDummyDeviceId";

// Non thread safe, life cycle is managed by owner.
BocaAppClient* g_instance = nullptr;

}  // namespace

// static
BocaAppClient* BocaAppClient::Get() {
  CHECK(g_instance);
  return g_instance;
}

bool BocaAppClient::HasInstance() {
  return g_instance;
}

BocaAppClient::BocaAppClient() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

BocaAppClient::~BocaAppClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void BocaAppClient::AddSessionManager(BocaSessionManager* session_manager) {
  // Session manager is created as profile service upon signin, so we can always
  // guarantee the active profile identity matches the session manager identity.
  auto* identity_manager = GetIdentityManager();
  CHECK(identity_manager);
  identity_manager->AddObserver(this);
  auto [it, was_inserted] =
      session_manager_map_.emplace(identity_manager, session_manager);
  CHECK(it->second == session_manager);
}

BocaSessionManager* BocaAppClient::GetSessionManager() {
  auto it = session_manager_map_.find(GetIdentityManager());
  CHECK(it != session_manager_map_.end());
  return it->second;
}

std::string BocaAppClient::GetDeviceId() {
  return kDummyDeviceId;
}

void BocaAppClient::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  // Remove observer here as boca_app_client detroys pretty-late(post
  // profile destroy).
  identity_manager->RemoveObserver(this);
  session_manager_map_.erase(identity_manager);
}

}  // namespace ash::boca
