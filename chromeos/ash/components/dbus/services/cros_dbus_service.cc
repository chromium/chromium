// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/system/sys_info.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"

namespace ash {

// The CrosDBusService implementation used in production, and unit tests.
class CrosDBusServiceImpl : public CrosDBusService {
 public:
  CrosDBusServiceImpl(dbus::Bus* bus,
                      const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      ServiceProviderList service_providers)
      : service_started_(false),
        origin_thread_id_(base::PlatformThread::CurrentId()),
        bus_(bus),
        service_name_(service_name),
        object_path_(object_path),
        service_providers_(std::move(service_providers)) {
    DCHECK(bus);
    DCHECK(!service_name_.empty());
    DCHECK(object_path_.IsValid());
  }

  CrosDBusServiceImpl(const CrosDBusServiceImpl&) = delete;
  CrosDBusServiceImpl& operator=(const CrosDBusServiceImpl&) = delete;

  ~CrosDBusServiceImpl() override = default;

  // Starts the D-Bus service.
  void Start() {
    // Make sure we're running on the origin thread (i.e. the UI thread in
    // production).
    DCHECK(OnOriginThread());
    DCHECK(!service_started_);

    // Methods must be exported before RequestOwnership is called:
    // https://crbug.com/874978
    exported_object_ = bus_->GetExportedObject(object_path_);
    for (const auto& provider : service_providers_)
      provider->Start(exported_object_);

    // There are some situations, described in http://crbug.com/234382#c27,
    // where processes on Linux can wind up stuck in an uninterruptible state
    // for tens of seconds. If this happens when Chrome is trying to exit, this
    // unkillable process can wind up clinging to ownership of |service_name_|
    // while the system is trying to restart the browser. This leads to a fatal
    // situation if we don't allow the new browser instance to replace the old
    // as the owner of |service_name_| as seen in http://crbug.com/234382.
    // Hence, REQUIRE_PRIMARY_ALLOW_REPLACEMENT.
    bus_->RequestOwnership(service_name_,
                           dbus::Bus::REQUIRE_PRIMARY_ALLOW_REPLACEMENT,
                           base::BindOnce(&CrosDBusServiceImpl::OnOwnership,
                                          base::Unretained(this)));

    service_started_ = true;
  }

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called when an ownership request is completed.
  void OnOwnership(const std::string& service_name,
                   bool success) {
    LOG_IF(FATAL, !success) << "Failed to own: " << service_name;
  }

  bool service_started_;
  base::PlatformThreadId origin_thread_id_;
  raw_ptr<dbus::Bus> bus_;
  std::string service_name_;
  dbus::ObjectPath object_path_;
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Service providers that form CrosDBusService.
  ServiceProviderList service_providers_;
};

// The stub CrosDBusService implementation used on Linux desktop,
// which does nothing as of now.
class CrosDBusServiceStubImpl : public CrosDBusService {
 public:
  CrosDBusServiceStubImpl() = default;

  CrosDBusServiceStubImpl(const CrosDBusServiceStubImpl&) = delete;
  CrosDBusServiceStubImpl& operator=(const CrosDBusServiceStubImpl&) = delete;

  ~CrosDBusServiceStubImpl() override = default;
};

// static
std::unique_ptr<CrosDBusService> CrosDBusService::Create(
    dbus::Bus* system_bus,
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    ServiceProviderList service_providers) {
  if (!system_bus)
    return std::make_unique<CrosDBusServiceStubImpl>();

  return CreateRealImpl(system_bus, service_name, object_path,
                        std::move(service_providers));
}

// static
CrosDBusService::ServiceProviderList CrosDBusService::CreateServiceProviderList(
    std::unique_ptr<ServiceProviderInterface> provider) {
  ServiceProviderList list;
  list.push_back(std::move(provider));
  return list;
}

// static
std::unique_ptr<CrosDBusService> CrosDBusService::CreateRealImpl(
    dbus::Bus* bus,
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    ServiceProviderList service_providers) {
  auto service = std::make_unique<CrosDBusServiceImpl>(
      bus, service_name, object_path, std::move(service_providers));
  service->Start();
  return std::move(service);
}

CrosDBusService::~CrosDBusService() = default;

CrosDBusService::CrosDBusService() = default;

CrosDBusService::ServiceProviderInterface::~ServiceProviderInterface() =
    default;

}  // namespace ash
