// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/hermes/fake_hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash {

namespace {

HermesManagerClient* g_instance = nullptr;

}  // namespace

// The HermesManagerClient implementation.
class HermesManagerClientImpl : public HermesManagerClient {
 public:
  explicit HermesManagerClientImpl(dbus::Bus* bus) {
    dbus::ObjectPath hermes_manager_path(hermes::kHermesManagerPath);
    object_proxy_ =
        bus->GetObjectProxy(hermes::kHermesServiceName, hermes_manager_path);
    object_proxy_->WaitForServiceToBeAvailable(
        base::BindOnce(&HermesManagerClientImpl::OnHermesAvailable,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  explicit HermesManagerClientImpl(const HermesManagerClient&) = delete;
  HermesManagerClientImpl& operator=(const HermesManagerClient&) = delete;
  ~HermesManagerClientImpl() override = default;

  // HermesManagerClient:
  const std::vector<dbus::ObjectPath>& GetAvailableEuiccs() override {
    if (properties_) {
      available_euiccs_ = properties_->available_euiccs().value();
    }
    return available_euiccs_;
  }

  TestInterface* GetTestInterface() override { return nullptr; }

 private:
  // Hermes Manager properties.
  class Properties : public dbus::PropertySet {
   public:
    Properties(dbus::ObjectProxy* object_proxy,
               const PropertyChangedCallback& callback)
        : dbus::PropertySet(object_proxy,
                            hermes::kHermesManagerInterface,
                            callback) {
      RegisterProperty(hermes::manager::kAvailableEuiccsProperty,
                       &available_euiccs_);
    }
    ~Properties() override = default;

    dbus::Property<std::vector<dbus::ObjectPath>>& available_euiccs() {
      return available_euiccs_;
    }

   private:
    // List of euicc objects available on the device.
    dbus::Property<std::vector<dbus::ObjectPath>> available_euiccs_;
  };

  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    for (auto& observer : observers()) {
      observer.OnAvailableEuiccListChanged();
    }
  }

  void OnHermesAvailable(bool service_is_available) {
    dbus::ObjectPath hermes_manager_path(hermes::kHermesManagerPath);
    properties_ = std::make_unique<Properties>(
        object_proxy_,
        base::BindRepeating(&HermesManagerClientImpl::OnPropertyChanged,
                            weak_ptr_factory_.GetWeakPtr(),
                            hermes_manager_path));
    properties_->ConnectSignals();
    properties_->GetAll();
  }

  raw_ptr<dbus::ObjectProxy> object_proxy_;
  std::unique_ptr<Properties> properties_;
  std::vector<dbus::ObjectPath> available_euiccs_;
  base::WeakPtrFactory<HermesManagerClientImpl> weak_ptr_factory_{this};
};

HermesManagerClient::HermesManagerClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

HermesManagerClient::~HermesManagerClient() {
  DCHECK_EQ(g_instance, this);
  for (auto& observer : observers()) {
    observer.OnShutdown();
  }
  g_instance = nullptr;
}

void HermesManagerClient::AddObserver(HermesManagerClient::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void HermesManagerClient::RemoveObserver(
    HermesManagerClient::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

// static
void HermesManagerClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  DCHECK(!g_instance);
  new HermesManagerClientImpl(bus);
}

// static
void HermesManagerClient::InitializeFake() {
  new FakeHermesManagerClient();
}

// static
void HermesManagerClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
HermesManagerClient* HermesManagerClient::Get() {
  return g_instance;
}

}  // namespace ash
