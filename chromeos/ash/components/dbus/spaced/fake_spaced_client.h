// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_FAKE_SPACED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_FAKE_SPACED_CLIENT_H_

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

  // FakeSpacedClient override:
  void GetFreeDiskSpace(const std::string& path,
                        GetSizeCallback callback) override;

  void GetTotalDiskSpace(const std::string& path,
                         GetSizeCallback callback) override;

  void GetRootDeviceSize(GetSizeCallback callback) override;

  base::WeakPtrFactory<FakeSpacedClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_FAKE_SPACED_CLIENT_H_
