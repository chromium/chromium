// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_service_registry.h"

#include "base/check.h"
#include "base/check_op.h"
#include "components/account_id/account_id.h"

namespace apps {

namespace {
AppServiceRegistry* g_instance = nullptr;
}  // namespace

AppServiceRegistry::AppServiceRegistry() {
  CHECK(!g_instance);
  g_instance = this;
}

AppServiceRegistry::~AppServiceRegistry() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

AppServiceRegistry* AppServiceRegistry::Get() {
  return g_instance;
}

AppService* AppServiceRegistry::Find(const AccountId& account_id) {
  CHECK(!account_id.empty());
  auto it = service_map_.find(account_id);
  return it == service_map_.end() ? nullptr : it->second;
}

void AppServiceRegistry::Register(const AccountId& account_id,
                                  AppService* app_service) {
  CHECK(!account_id.empty());
  CHECK(app_service);
  CHECK(service_map_.try_emplace(account_id, app_service).second);
}

void AppServiceRegistry::Unregister(const AccountId& account_id) {
  CHECK(!account_id.empty());
  CHECK_GT(service_map_.erase(account_id), 0u);
}

}  // namespace apps
