// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/arc_data_snapshotd_client.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/ash/components/dbus/arc/fake_arc_data_snapshotd_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/arc-data-snapshotd/dbus-constants.h"

namespace ash {

namespace {

ArcDataSnapshotdClient* g_instance = nullptr;

void OnBoolMethodCallback(chromeos::VoidDBusMethodCallback callback,
                          dbus::Response* response) {
  if (!response) {
    std::move(callback).Run(false /* success */);
    return;
  }
  dbus::MessageReader reader(response);
  bool success;
  if (!reader.PopBool(&success)) {
    std::move(callback).Run(false /* success */);
    return;
  }
  std::move(callback).Run(success);
}

void OnLoadSnapshotMethodCallback(
    ArcDataSnapshotdClient::LoadSnapshotMethodCallback callback,
    dbus::Response* response) {
  if (!response) {
    std::move(callback).Run(false /* success */, false /* last */);
    return;
  }
  dbus::MessageReader reader(response);
  bool success;
  if (!reader.PopBool(&success)) {
    std::move(callback).Run(false /* success */, false /* last */);
    return;
  }
  bool last;
  if (!reader.PopBool(&last)) {
    std::move(callback).Run(success, false /* last */);
    return;
  }
  std::move(callback).Run(success, last);
}

class ArcDataSnapshotdClientImpl : public ArcDataSnapshotdClient {
 public:
  ArcDataSnapshotdClientImpl() {}

  ~ArcDataSnapshotdClientImpl() override = default;

  ArcDataSnapshotdClientImpl(const ArcDataSnapshotdClientImpl&) = delete;
  ArcDataSnapshotdClientImpl& operator=(const ArcDataSnapshotdClientImpl&) =
      delete;

  void GenerateKeyPair(chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        arc::data_snapshotd::kArcDataSnapshotdServiceInterface,
        arc::data_snapshotd::kGenerateKeyPairMethod);
    dbus::MessageWriter writer(&method_call);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnBoolMethodCallback, std::move(callback)));
  }

  void ClearSnapshot(bool last,
                     chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        arc::data_snapshotd::kArcDataSnapshotdServiceInterface,
        arc::data_snapshotd::kClearSnapshotMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(last);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnBoolMethodCallback, std::move(callback)));
  }

  void TakeSnapshot(const std::string& account_id,
                    chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        arc::data_snapshotd::kArcDataSnapshotdServiceInterface,
        arc::data_snapshotd::kTakeSnapshotMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(account_id);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnBoolMethodCallback, std::move(callback)));
  }

  void LoadSnapshot(const std::string& account_id,
                    base::OnceCallback<void(bool, bool)> callback) override {
    dbus::MethodCall method_call(
        arc::data_snapshotd::kArcDataSnapshotdServiceInterface,
        arc::data_snapshotd::kLoadSnapshotMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(account_id);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnLoadSnapshotMethodCallback, std::move(callback)));
  }

  void Update(int percent, chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        arc::data_snapshotd::kArcDataSnapshotdServiceInterface,
        arc::data_snapshotd::kUpdateMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(percent);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnBoolMethodCallback, std::move(callback)));
  }

  void ConnectToUiCancelledSignal(
      base::RepeatingClosure signal_callback,
      base::OnceCallback<void(bool)> on_connected_callback) override {
    signal_callback_ = std::move(signal_callback);
    proxy_->ConnectToSignal(
        arc::data_snapshotd::kArcDataSnapshotdServiceInterface,
        arc::data_snapshotd::kUiCancelled,
        base::BindRepeating(
            &ArcDataSnapshotdClientImpl::OnUiCancelledSignalCallback,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(
            &ArcDataSnapshotdClientImpl::OnUiCancelledSignalConnectedCallback,
            weak_ptr_factory_.GetWeakPtr(), std::move(on_connected_callback)));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        arc::data_snapshotd::kArcDataSnapshotdServiceName,
        dbus::ObjectPath(arc::data_snapshotd::kArcDataSnapshotdServicePath));
  }

 private:
  void OnUiCancelledSignalCallback(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(),
              arc::data_snapshotd::kArcDataSnapshotdServiceInterface);
    DCHECK_EQ(signal->GetMember(), arc::data_snapshotd::kUiCancelled);
    DCHECK(!signal_callback_.is_null());

    signal_callback_.Run();
  }

  void OnUiCancelledSignalConnectedCallback(
      base::OnceCallback<void(bool)> on_connected_callback,
      const std::string& interface_name,
      const std::string& signal_name,
      bool success) {
    DCHECK_EQ(interface_name,
              arc::data_snapshotd::kArcDataSnapshotdServiceInterface);
    DCHECK_EQ(signal_name, arc::data_snapshotd::kUiCancelled);

    std::move(on_connected_callback).Run(success);
  }

  // Owned by the D-Bus implementation, who outlives this class.
  dbus::ObjectProxy* proxy_ = nullptr;

  // Callback passed to |ConnectToUiCancelledSignal| as a |signal_callback|.
  base::RepeatingClosure signal_callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcDataSnapshotdClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
ArcDataSnapshotdClient* ArcDataSnapshotdClient::Get() {
  return g_instance;
}

// static
void ArcDataSnapshotdClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ArcDataSnapshotdClientImpl())->Init(bus);
}

// static
void ArcDataSnapshotdClient::InitializeFake() {
  (new FakeArcDataSnapshotdClient())->Init(nullptr);
}

// static
void ArcDataSnapshotdClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

ArcDataSnapshotdClient::ArcDataSnapshotdClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ArcDataSnapshotdClient::~ArcDataSnapshotdClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
