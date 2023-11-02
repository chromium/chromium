// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_VIRTUAL_FILE_PROVIDER_FAKE_VIRTUAL_FILE_PROVIDER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_VIRTUAL_FILE_PROVIDER_FAKE_VIRTUAL_FILE_PROVIDER_CLIENT_H_

#include <string>
#include <utility>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/virtual_file_provider/virtual_file_provider_client.h"

namespace ash {

class COMPONENT_EXPORT(ASH_DBUS_VIRTUAL_FILE_PROVIDER)
    FakeVirtualFileProviderClient : public VirtualFileProviderClient {
 public:
  FakeVirtualFileProviderClient();

  FakeVirtualFileProviderClient(const FakeVirtualFileProviderClient&) = delete;
  FakeVirtualFileProviderClient& operator=(
      const FakeVirtualFileProviderClient&) = delete;

  ~FakeVirtualFileProviderClient() override;

  // chromeos::DBusClient override.
  void Init(dbus::Bus* bus) override;

  // VirtualFileProviderClient overrides:
  void GenerateVirtualFileId(int64_t size,
                             GenerateVirtualFileIdCallback callback) override;
  void OpenFileById(const std::string& id,
                    OpenFileByIdCallback callback) override;

  void set_expected_size(int64_t size) { expected_size_ = size; }
  void set_result_id(const std::string& id) { result_id_ = id; }
  void set_result_fd(base::ScopedFD fd) { result_fd_ = std::move(fd); }

 private:
  int64_t expected_size_ = 0;  // Expectation for GenerateVirtualFileId.
  std::string result_id_;      // Returned by GenerateVirtualFileId.
  base::ScopedFD result_fd_;   // Returned by OpenFileById.
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_VIRTUAL_FILE_PROVIDER_FAKE_VIRTUAL_FILE_PROVIDER_CLIENT_H_
