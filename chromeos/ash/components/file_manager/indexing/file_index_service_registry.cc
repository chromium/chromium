// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/file_index_service_registry.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/file_manager/indexing/file_index.h"
#include "chromeos/ash/components/file_manager/indexing/file_index_service.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace ash::file_manager {
namespace {

FileIndexServiceRegistry* g_instance = nullptr;

}  // namespace

// static
FileIndexServiceRegistry* FileIndexServiceRegistry::Get() {
  CHECK(g_instance);
  return g_instance;
}

FileIndexServiceRegistry::FileIndexServiceRegistry(
    user_manager::UserManager* user_manager)
    : user_manager_(user_manager) {
  CHECK(!g_instance);
  g_instance = this;
  observation_.Observe(user_manager);

  // For the current user the Observe above is already too late, force its
  // initialization here.
  if (user_manager->GetActiveUser()) {
    OnUserProfileCreated(*user_manager->GetActiveUser());
  }
}

FileIndexServiceRegistry::~FileIndexServiceRegistry() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void FileIndexServiceRegistry::Shutdown() {
  map_.clear();
}

FileIndexService* FileIndexServiceRegistry::GetFileIndexService(
    const AccountId& account_id) {
  auto it = map_.find(account_id);
  if (it == map_.end()) {
    return nullptr;
  }

  return &it->second;
}

void FileIndexServiceRegistry::OnUserProfileCreated(
    const user_manager::User& user) {
  auto account_id = user.GetAccountId();
  auto it = map_.find(account_id);
  if (it != map_.end()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(::ash::features::kFilesMaterializedViews)) {
    return;
  }
  base::FilePath ash_home_dir =
      ash::BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
          user.username_hash());

  auto r = map_.emplace(account_id, ash_home_dir);
  if (r.second) {
    auto& index = r.first->second;
    index.Init(base::BindOnce([](OpResults result) {
      if (result != OpResults::kSuccess) {
        LOG(ERROR) << "Failed to initialize the file index: " << result;
      }
    }));
  }
}
}  // namespace ash::file_manager
