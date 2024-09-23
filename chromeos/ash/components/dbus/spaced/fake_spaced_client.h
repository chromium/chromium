// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_FAKE_SPACED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_FAKE_SPACED_CLIENT_H_

#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"

namespace ash {

// A fake implementation of SpacedClient.
class COMPONENT_EXPORT(SPACED_CLIENT) FakeSpacedClient : public SpacedClient {
 public:
  FakeSpacedClient();
  ~FakeSpacedClient() override;

  // Not copyable or movable.
  FakeSpacedClient(const FakeSpacedClient&) = delete;
  FakeSpacedClient& operator=(const FakeSpacedClient&) = delete;

  // Returns the fake global instance if initialized. May return null.
  static FakeSpacedClient* Get();

  // FakeSpacedClient override:
  void GetFreeDiskSpace(const std::string& path,
                        GetSizeCallback callback) override;

  void GetTotalDiskSpace(const std::string& path,
                         GetSizeCallback callback) override;

  void GetRootDeviceSize(GetSizeCallback callback) override;

  void IsQuotaSupported(const std::string& path,
                        BoolCallback callback) override;

  void GetQuotaCurrentSpaceForUid(const std::string& path,
                                  uint32_t uid,
                                  GetSizeCallback callback) override;
  void GetQuotaCurrentSpaceForGid(const std::string& path,
                                  uint32_t gid,
                                  GetSizeCallback callback) override;
  void GetQuotaCurrentSpaceForProjectId(const std::string& path,
                                        uint32_t project_id,
                                        GetSizeCallback callback) override;
  void GetQuotaCurrentSpacesForIds(const std::string& path,
                                   const std::vector<uint32_t>& uids,
                                   const std::vector<uint32_t>& gids,
                                   const std::vector<uint32_t>& project_ids,
                                   GetSpacesForIdsCallback callback) override;

  void SendStatefulDiskSpaceUpdate(const Observer::SpaceEvent& event);

  void set_free_disk_space(std::optional<int64_t> space) {
    free_disk_space_ = space;
  }

  void set_total_disk_space(std::optional<int64_t> space) {
    total_disk_space_ = space;
  }

  void set_root_device_size(std::optional<int64_t> size) {
    root_device_size_ = size;
  }

  void set_quota_supported(std::optional<bool> quota_supported) {
    quota_supported_ = quota_supported;
  }

  void set_quota_current_space_uid(uint32_t uid, int64_t space) {
    quota_current_space_uid_[uid] = space;
  }

  void set_quota_current_space_gid(uint32_t gid, int64_t space) {
    quota_current_space_gid_[gid] = space;
  }

  void set_quota_current_space_project_id(uint32_t project_id, int64_t space) {
    quota_current_space_project_id_[project_id] = space;
  }

  void set_connected(bool connected) { connected_ = connected; }

 private:
  std::optional<int64_t> free_disk_space_;
  std::optional<int64_t> total_disk_space_;
  std::optional<int64_t> root_device_size_;
  std::optional<bool> quota_supported_;
  std::map<uint32_t, int64_t> quota_current_space_uid_;
  std::map<uint32_t, int64_t> quota_current_space_gid_;
  std::map<uint32_t, int64_t> quota_current_space_project_id_;

  base::WeakPtrFactory<FakeSpacedClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_FAKE_SPACED_CLIENT_H_
