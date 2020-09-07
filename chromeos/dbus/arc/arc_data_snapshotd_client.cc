// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/arc/arc_data_snapshotd_client.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/arc-data-snapshotd/dbus-constants.h"

namespace chromeos {

namespace {

void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                      dbus::Response* response) {
  std::move(callback).Run(response != nullptr);
}

class ArcDataSnapshotdClientImpl : public ArcDataSnapshotdClient {
 public:
  ArcDataSnapshotdClientImpl() {}

  ~ArcDataSnapshotdClientImpl() override = default;

  ArcDataSnapshotdClientImpl(const ArcDataSnapshotdClientImpl&) = delete;
  ArcDataSnapshotdClientImpl& operator=(const ArcDataSnapshotdClientImpl&) =
      delete;

  void GenerateKeyPair(VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        arc::data_snapshotd::kArcDataSnapshotdServiceInterface,
        arc::data_snapshotd::kGenerateKeyPairMethod);
    dbus::MessageWriter writer(&method_call);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&OnVoidDBusMethod, std::move(callback)));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        arc::data_snapshotd::kArcDataSnapshotdServiceName,
        dbus::ObjectPath(arc::data_snapshotd::kArcDataSnapshotdServicePath));
  }

 private:
  // Owned by the D-Bus implementation, who outlives this class.
  dbus::ObjectProxy* proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcDataSnapshotdClientImpl> weak_ptr_factory_{this};
};

}  // namespace

ArcDataSnapshotdClient::ArcDataSnapshotdClient() = default;

ArcDataSnapshotdClient::~ArcDataSnapshotdClient() = default;

std::unique_ptr<ArcDataSnapshotdClient> ArcDataSnapshotdClient::Create() {
  return std::make_unique<ArcDataSnapshotdClientImpl>();
}

}  // namespace chromeos
