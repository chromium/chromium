// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hps/hps_dbus_client.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/hps/fake_hps_dbus_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/hps/dbus-constants.h"

namespace chromeos {

namespace {

HpsDBusClient* g_instance = nullptr;

// Extracts the HPS notify data out of a DBus response.
absl::optional<bool> UnwrapHpsNotifyResult(dbus::Response* response) {
  if (response == nullptr) {
    return absl::nullopt;
  }

  dbus::MessageReader reader(response);
  bool result = false;
  if (!reader.PopBool(&result)) {
    LOG(ERROR) << "Invalid DBus response data";
    return absl::nullopt;
  }

  return result;
}

class HpsDBusClientImpl : public HpsDBusClient {
 public:
  explicit HpsDBusClientImpl(dbus::Bus* bus)
      : hps_proxy_(bus->GetObjectProxy(hps::kHpsServiceName,
                                       dbus::ObjectPath(hps::kHpsServicePath))),
        weak_ptr_factory_(this) {
    // Connect to HpsNotifyChanged signal.
    hps_proxy_->ConnectToSignal(
        hps::kHpsServiceInterface, hps::kHpsNotifyChanged,
        base::BindRepeating(&HpsDBusClientImpl::HpsNotifyChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&HpsDBusClientImpl::HpsNotifyChangedConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  ~HpsDBusClientImpl() override = default;

  HpsDBusClientImpl(const HpsDBusClientImpl&) = delete;
  HpsDBusClientImpl& operator=(const HpsDBusClientImpl&) = delete;

  // Called when the notify changed signal is received.
  void HpsNotifyChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    bool state = false;
    if (!reader.PopBool(&state)) {
      LOG(ERROR) << "Invalid HpsNotifyChanged signal: " << signal->ToString();
      return;
    }

    // Notify observers of state changed.
    for (auto& observer : observers_) {
      observer.OnHpsNotifyChanged(state);
    }
  }

  // Called when the HpsNotifyChanged signal is initially connected.
  void HpsNotifyChangedConnected(const std::string& /* interface_name */,
                                 const std::string& /* signal_name */,
                                 bool success) {
    LOG_IF(ERROR, !success) << "Failed to connect to HpsNotifyChanged signal.";
  }

  // HpsDBusClient:

  void GetResultHpsNotify(GetResultHpsNotifyCallback cb) override {
    dbus::MethodCall method_call(hps::kHpsServiceInterface,
                                 hps::kGetResultHpsNotify);
    dbus::MessageWriter writer(&method_call);
    hps_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UnwrapHpsNotifyResult).Then(std::move(cb)));
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

 private:
  dbus::ObjectProxy* const hps_proxy_;

  base::ObserverList<Observer> observers_;

  // Must be last class member.
  base::WeakPtrFactory<HpsDBusClientImpl> weak_ptr_factory_{this};
};

}  // namespace

HpsDBusClient::Observer::Observer() = default;
HpsDBusClient::Observer::~Observer() = default;

HpsDBusClient::HpsDBusClient() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

HpsDBusClient::~HpsDBusClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
void HpsDBusClient::Initialize(dbus::Bus* bus) {
  DCHECK_NE(bus, nullptr);
  new HpsDBusClientImpl(bus);
}

// static
void HpsDBusClient::InitializeFake() {
  new FakeHpsDBusClient();
}

// static
void HpsDBusClient::Shutdown() {
  DCHECK_NE(g_instance, nullptr);
  delete g_instance;
}

// static
HpsDBusClient* HpsDBusClient::Get() {
  return g_instance;
}

}  // namespace chromeos
