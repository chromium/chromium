// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_
#define CHROMEOS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/resourced/resourced_client.h"

namespace chromeos {

class COMPONENT_EXPORT(RESOURCED) FakeResourcedClient : public ResourcedClient {
 public:
  FakeResourcedClient();
  ~FakeResourcedClient() override;

  FakeResourcedClient(const FakeResourcedClient&) = delete;
  FakeResourcedClient& operator=(const FakeResourcedClient&) = delete;

  // ResourcedClient:
  void GetAvailableMemoryKB(DBusMethodCallback<uint64_t> callback) override;

  // Get memory margins.
  void GetMemoryMarginsKB(
      DBusMethodCallback<ResourcedClient::MemoryMarginsKB> callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_
