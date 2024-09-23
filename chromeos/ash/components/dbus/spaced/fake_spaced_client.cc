// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"

#include <map>
#include <vector>

namespace ash {

namespace {

FakeSpacedClient* g_instance = nullptr;

int64_t GetSpace(uint32_t id, const std::map<uint32_t, int64_t>& curspaces) {
  auto iter = curspaces.find(id);
  return iter == curspaces.end() ? 0 : iter->second;
}

std::map<uint32_t, int64_t> ConstructSpaceMap(
    const std::vector<uint32_t>& ids,
    const std::map<uint32_t, int64_t>& curspaces) {
  std::map<uint32_t, int64_t> quota_map;
  for (const auto& id : ids) {
    quota_map[id] = GetSpace(id, curspaces);
  }
  return quota_map;
}

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
  std::move(callback).Run(GetSpace(uid, quota_current_space_uid_));
}

void FakeSpacedClient::GetQuotaCurrentSpaceForGid(const std::string& path,
                                                  uint32_t gid,
                                                  GetSizeCallback callback) {
  std::move(callback).Run(GetSpace(gid, quota_current_space_gid_));
}

void FakeSpacedClient::GetQuotaCurrentSpaceForProjectId(
    const std::string& path,
    uint32_t project_id,
    GetSizeCallback callback) {
  std::move(callback).Run(
      GetSpace(project_id, quota_current_space_project_id_));
}

void FakeSpacedClient::GetQuotaCurrentSpacesForIds(
    const std::string& path,
    const std::vector<uint32_t>& uids,
    const std::vector<uint32_t>& gids,
    const std::vector<uint32_t>& project_ids,
    GetSpacesForIdsCallback callback) {
  std::move(callback).Run(SpaceMaps(
      ConstructSpaceMap(uids, quota_current_space_uid_),
      ConstructSpaceMap(gids, quota_current_space_gid_),
      ConstructSpaceMap(project_ids, quota_current_space_project_id_)));
}

void FakeSpacedClient::SendStatefulDiskSpaceUpdate(
    const Observer::SpaceEvent& event) {
  for (Observer& observer : observers_) {
    observer.OnSpaceUpdate(event);
  }
}

}  // namespace ash
