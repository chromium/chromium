// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/os_install/os_install_client.h"

#include <optional>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/os_install/fake_os_install_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

OsInstallClient* g_instance = nullptr;

std::optional<OsInstallClient::Status> ParseStatus(const std::string& str) {
  if (str == os_install_service::kStatusInProgress)
    return OsInstallClient::Status::InProgress;
  if (str == os_install_service::kStatusSucceeded)
    return OsInstallClient::Status::Succeeded;
  if (str == os_install_service::kStatusFailed)
    return OsInstallClient::Status::Failed;
  if (str == os_install_service::kStatusNoDestinationDeviceFound)
    return OsInstallClient::Status::NoDestinationDeviceFound;

  LOG(ERROR) << "Invalid status: " << str;
  return std::nullopt;
}

class OsInstallClientImpl : public OsInstallClient {
 public:
  OsInstallClientImpl() = default;
  ~OsInstallClientImpl() override = default;
  OsInstallClientImpl(const OsInstallClientImpl&) = delete;
  OsInstallClientImpl& operator=(const OsInstallClientImpl&) = delete;

  void Init(dbus::Bus* bus);

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  TestInterface* GetTestInterface() override;
  void StartOsInstall() override;

 private:
  void NotifyStatusChanged(std::optional<Status> status,
                           const std::string& service_log);
  void HandleStartResponse(dbus::Response* response);
  void StatusUpdateReceived(dbus::Signal* signal);
  void StatusUpdateConnected(const std::string& interface_name,
                             const std::string& signal_name,
                             bool success);

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<OsInstallClientImpl> weak_factory_{this};
};

void OsInstallClientImpl::Init(dbus::Bus* bus) {
  proxy_ = bus->GetObjectProxy(
      os_install_service::kOsInstallServiceServiceName,
      dbus::ObjectPath(os_install_service::kOsInstallServiceServicePath));

  proxy_->ConnectToSignal(
      os_install_service::kOsInstallServiceInterface,
      os_install_service::kSignalOsInstallStatusChanged,
      base::BindRepeating(&OsInstallClientImpl::StatusUpdateReceived,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&OsInstallClientImpl::StatusUpdateConnected,
                     weak_factory_.GetWeakPtr()));
}

void OsInstallClientImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OsInstallClientImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool OsInstallClientImpl::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

OsInstallClient::TestInterface* OsInstallClientImpl::GetTestInterface() {
  return nullptr;
}

void OsInstallClientImpl::StartOsInstall() {
  dbus::MethodCall method_call(os_install_service::kOsInstallServiceInterface,
                               os_install_service::kMethodStartOsInstall);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::BindOnce(&OsInstallClientImpl::HandleStartResponse,
                                    weak_factory_.GetWeakPtr()));
}

void OsInstallClientImpl::NotifyStatusChanged(std::optional<Status> status,
                                              const std::string& service_log) {
  if (!status) {
    status = Status::Failed;
  }

  for (auto& observer : observers_) {
    observer.StatusChanged(*status, service_log);
  }
}

void OsInstallClientImpl::HandleStartResponse(dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "Invalid response";
    NotifyStatusChanged(Status::Failed, /*service_log=*/"");
    return;
  }

  dbus::MessageReader reader(response);
  std::string status_str;
  if (!reader.PopString(&status_str)) {
    LOG(ERROR) << "Missing status";
    NotifyStatusChanged(Status::Failed, /*service_log=*/"");
    return;
  }

  NotifyStatusChanged(ParseStatus(status_str), /*service_log=*/"");
}

void OsInstallClientImpl::StatusUpdateReceived(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);

  // Read and parse the status.
  std::string status_str;
  if (!reader.PopString(&status_str)) {
    LOG(ERROR) << "Missing status";
    return;
  }
  const auto status = ParseStatus(status_str);
  if (!status) {
    return;
  }

  // Read the service log.
  std::string service_log;
  if (!reader.PopString(&service_log)) {
    LOG(ERROR) << "Missing service_log";
    return;
  }

  NotifyStatusChanged(*status, service_log);
}

void OsInstallClientImpl::StatusUpdateConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  LOG_IF(WARNING, !success) << "Failed to connect to status updated signal.";
}

}  // namespace

OsInstallClient::OsInstallClient() {
  CHECK(!g_instance);
  g_instance = this;
}

OsInstallClient::~OsInstallClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void OsInstallClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new OsInstallClientImpl())->Init(bus);
}

// static
void OsInstallClient::InitializeFake() {
  new FakeOsInstallClient();
}

// static
void OsInstallClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
OsInstallClient* OsInstallClient::Get() {
  return g_instance;
}

}  // namespace ash
