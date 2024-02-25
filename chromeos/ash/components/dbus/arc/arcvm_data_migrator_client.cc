// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/arcvm_data_migrator_client.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/arc/fake_arcvm_data_migrator_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ArcVmDataMigratorClient* g_instance = nullptr;

void OnSignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
  DCHECK_EQ(interface_name, arc::data_migrator::kArcVmDataMigratorInterface);
  LOG_IF(DFATAL, !success) << "Failed to connect to D-Bus signal; interface: "
                           << interface_name << "; signal: " << signal_name;
}

class ArcVmDataMigratorClientImpl : public ArcVmDataMigratorClient {
 public:
  explicit ArcVmDataMigratorClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(
            arc::data_migrator::kArcVmDataMigratorServiceName,
            dbus::ObjectPath(
                arc::data_migrator::kArcVmDataMigratorServicePath))) {
    proxy_->ConnectToSignal(
        arc::data_migrator::kArcVmDataMigratorInterface,
        arc::data_migrator::kMigrationProgressSignal,
        base::BindRepeating(&ArcVmDataMigratorClientImpl::OnMigrationProgress,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&OnSignalConnected));
  }

  ~ArcVmDataMigratorClientImpl() override = default;

  ArcVmDataMigratorClientImpl(const ArcVmDataMigratorClientImpl&) = delete;
  ArcVmDataMigratorClientImpl& operator=(const ArcVmDataMigratorClientImpl&) =
      delete;

  // ArcVmDataMigratorClient overrides:
  void HasDataToMigrate(
      const arc::data_migrator::HasDataToMigrateRequest& request,
      chromeos::DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(
        arc::data_migrator::kArcVmDataMigratorInterface,
        arc::data_migrator::kHasDataToMigrateMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcVmDataMigratorClientImpl::OnBoolMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetAndroidDataInfo(
      const arc::data_migrator::GetAndroidDataInfoRequest& request,
      GetAndroidDataInfoCallback callback) override {
    dbus::MethodCall method_call(
        arc::data_migrator::kArcVmDataMigratorInterface,
        arc::data_migrator::kGetAndroidDataInfoMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);
    proxy_->CallMethod(
        &method_call,
        kArcVmDataMigratorGetAndroidDataInfoTimeout.InMilliseconds(),
        base::BindOnce(
            &ArcVmDataMigratorClientImpl::OnGetAndroidDataInfoResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartMigration(const arc::data_migrator::StartMigrationRequest& request,
                      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        arc::data_migrator::kArcVmDataMigratorInterface,
        arc::data_migrator::kStartMigrationMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcVmDataMigratorClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

 private:
  void OnMigrationProgress(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    arc::data_migrator::DataMigrationProgress proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse DataMigrationProgress protobuf from "
                    "D-Bus signal";
      return;
    }
    for (Observer& observer : observers_) {
      observer.OnDataMigrationProgress(proto);
    }
  }

  void OnVoidMethod(chromeos::VoidDBusMethodCallback callback,
                    dbus::Response* response) {
    std::move(callback).Run(response);
  }

  void OnBoolMethod(chromeos::DBusMethodCallback<bool> callback,
                    dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    bool result = false;
    if (!reader.PopBool(&result)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(result);
  }

  void OnGetAndroidDataInfoResponse(GetAndroidDataInfoCallback callback,
                                    dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    arc::data_migrator::GetAndroidDataInfoResponse proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(std::move(proto));
  }

  base::ObserverList<Observer> observers_;
  raw_ptr<dbus::ObjectProxy> proxy_;
  base::WeakPtrFactory<ArcVmDataMigratorClientImpl> weak_ptr_factory_{this};
};

}  // namespace

ArcVmDataMigratorClient::ArcVmDataMigratorClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ArcVmDataMigratorClient::~ArcVmDataMigratorClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ArcVmDataMigratorClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new ArcVmDataMigratorClientImpl(bus);
}

// static
void ArcVmDataMigratorClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test (to
  // allow test properties to be set).
  if (!FakeArcVmDataMigratorClient::Get())
    new FakeArcVmDataMigratorClient();
}

// static
void ArcVmDataMigratorClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ArcVmDataMigratorClient* ArcVmDataMigratorClient::Get() {
  return g_instance;
}

}  // namespace ash
