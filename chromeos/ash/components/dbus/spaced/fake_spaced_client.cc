// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"

#include <map>

namespace ash {

namespace {

FakeSpacedClient* g_instance = nullptr;

}  // namespace

// static
FakeSpacedClient* FakeSpacedClient::Get() {
  return g_instance;
}

FakeSpacedClient::FakeSpacedClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeSpacedClient::~FakeSpacedClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void FakeSpacedClient::GetFreeDiskSpace(const std::string& path,
                                        GetSizeCallback callback) {
  std::move(callback).Run(free_disk_space_);
}

void FakeSpacedClient::GetTotalDiskSpace(const std::string& path,
                                         GetSizeCallback callback) {
  std::move(callback).Run(total_disk_space_);
}

void FakeSpacedClient::GetRootDeviceSize(GetSizeCallback callback) {
  std::move(callback).Run(root_device_size_);
}

void FakeSpacedClient::IsQuotaSupported(const std::string& path,
                                        BoolCallback callback) {
  std::move(callback).Run(quota_supported_);
}

void FakeSpacedClient::GetQuotaCurrentSpaceForUid(const std::string& path,
                                                  uint32_t uid,
                                                  GetSizeCallback callback) {
  auto iter = quota_current_space_uid_.find(uid);
  if (iter == quota_current_space_uid_.end()) {
    std::move(callback).Run(0);
  } else {
    std::move(callback).Run(iter->second);
  }
}

void FakeSpacedClient::GetQuotaCurrentSpaceForGid(const std::string& path,
                                                  uint32_t gid,
                                                  GetSizeCallback callback) {
  auto iter = quota_current_space_gid_.find(gid);
  if (iter == quota_current_space_gid_.end()) {
    std::move(callback).Run(0);
  } else {
    std::move(callback).Run(iter->second);
  }
}

void FakeSpacedClient::GetQuotaCurrentSpaceForProjectId(
    const std::string& path,
    uint32_t project_id,
    GetSizeCallback callback) {
  auto iter = quota_current_space_project_id_.find(project_id);
  if (iter == quota_current_space_project_id_.end()) {
    std::move(callback).Run(0);
  } else {
    std::move(callback).Run(iter->second);
  }
}

}  // namespace ash
