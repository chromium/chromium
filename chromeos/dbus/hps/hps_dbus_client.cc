// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hps/hps_dbus_client.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
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

// Extracts result data out of a DBus response.
absl::optional<hps::HpsResult> UnwrapHpsResult(dbus::Response* response) {
  if (response == nullptr) {
    return absl::nullopt;
  }

  dbus::MessageReader reader(response);
  hps::HpsResultProto result;
  if (!reader.PopArrayOfBytesAsProto(&result)) {
    LOG(ERROR) << "Invalid DBus response data";
    return absl::nullopt;
  }

  return result.value();
}

class HpsDBusClientImpl : public HpsDBusClient {
 public:
  explicit HpsDBusClientImpl(dbus::Bus* bus)
      : hps_proxy_(bus->GetObjectProxy(hps::kHpsServiceName,
                                       dbus::ObjectPath(hps::kHpsServicePath))),
        weak_ptr_factory_(this) {
    // Connect to HpsSenseChanged signal.
    hps_proxy_->ConnectToSignal(
        hps::kHpsServiceInterface, hps::kHpsSenseChanged,
        base::BindRepeating(&HpsDBusClientImpl::HpsSenseChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&HpsDBusClientImpl::HpsSenseChangedConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Connect to HpsNotifyChanged signal.
    hps_proxy_->ConnectToSignal(
        hps::kHpsServiceInterface, hps::kHpsNotifyChanged,
        base::BindRepeating(&HpsDBusClientImpl::HpsNotifyChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&HpsDBusClientImpl::HpsNotifyChangedConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor daemon restarts.
    hps_proxy_->SetNameOwnerChangedCallback(base::BindRepeating(
        &HpsDBusClientImpl::NameOwnerChanged, weak_ptr_factory_.GetWeakPtr()));
  }

  ~HpsDBusClientImpl() override = default;

  HpsDBusClientImpl(const HpsDBusClientImpl&) = delete;
  HpsDBusClientImpl& operator=(const HpsDBusClientImpl&) = delete;

  // Called when user presence signal is received.
  void HpsSenseChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    hps::HpsResultProto result;
    if (!reader.PopArrayOfBytesAsProto(&result)) {
      LOG(ERROR) << "Invalid HpsSenseChanged signal: " << signal->ToString();
      return;
    }

    // Notify observers of state change.
    for (auto& observer : observers_) {
      observer.OnHpsSenseChanged(result.value());
    }
  }

  // Called when snooping signal is received.
  void HpsNotifyChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    hps::HpsResultProto result;
    if (!reader.PopArrayOfBytesAsProto(&result)) {
      LOG(ERROR) << "Invalid HpsNotifyChanged signal: " << signal->ToString();
      return;
    }

    // Notify observers of state change.
    for (auto& observer : observers_) {
      observer.OnHpsNotifyChanged(result.value());
    }
  }

  // Called with a non-empty |new_owner| when the service is restarted, or an
  // empty |new_owner| when the service is shutdown.
  void NameOwnerChanged(const std::string& /* old_owner */,
                        const std::string& new_owner) {
    const auto method =
        new_owner.empty() ? &Observer::OnShutdown : &Observer::OnRestart;
    for (auto& observer : observers_) {
      (observer.*method)();
    }
  }

  // Called when the HpsSenseChanged signal is initially connected.
  void HpsSenseChangedConnected(const std::string& /* interface_name */,
                                const std::string& /* signal_name */,
                                bool success) {
    LOG_IF(ERROR, !success) << "Failed to connect to HpsSenseChanged signal.";
  }

  // Called when the HpsNotifyChanged signal is initially connected.
  void HpsNotifyChangedConnected(const std::string& /* interface_name */,
                                 const std::string& /* signal_name */,
                                 bool success) {
    LOG_IF(ERROR, !success) << "Failed to connect to HpsNotifyChanged signal.";
  }

  // HpsDBusClient:
  void GetResultHpsSense(GetResultCallback cb) override {
    dbus::MethodCall method_call(hps::kHpsServiceInterface,
                                 hps::kGetResultHpsSense);
    dbus::MessageWriter writer(&method_call);
    hps_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UnwrapHpsResult).Then(std::move(cb)));
  }

  void GetResultHpsNotify(GetResultCallback cb) override {
    dbus::MethodCall method_call(hps::kHpsServiceInterface,
                                 hps::kGetResultHpsNotify);
    dbus::MessageWriter writer(&method_call);
    hps_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UnwrapHpsResult).Then(std::move(cb)));
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void EnableHpsSense(const hps::FeatureConfig& config) override {
    EnableHpsFeature(hps::kEnableHpsSense, config);
  }

  void DisableHpsSense() override { DisableHpsFeature(hps::kDisableHpsSense); }

  void EnableHpsNotify(const hps::FeatureConfig& config) override {
    EnableHpsFeature(hps::kEnableHpsNotify, config);
  }

  void DisableHpsNotify() override {
    DisableHpsFeature(hps::kDisableHpsNotify);
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    hps_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

 private:
  // Send a method call to HpsDBus with given method name and config.
  void EnableHpsFeature(const std::string& method_name,
                        const hps::FeatureConfig& config) {
    dbus::MethodCall method_call(hps::kHpsServiceInterface, method_name);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(config)) {
      LOG(ERROR) << "Failed to encode protobuf for " << method_name;
    } else {
      hps_proxy_->CallMethod(&method_call,
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                             base::DoNothing());
    }
  }

  // Send a method call to HpsDBus with given method name.
  void DisableHpsFeature(const std::string& method_name) {
    dbus::MethodCall method_call(hps::kHpsServiceInterface, method_name);
    hps_proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                           base::DoNothing());
  }

  dbus::ObjectProxy* const hps_proxy_;

  base::ObserverList<Observer> observers_;

  // Must be last class member.
  base::WeakPtrFactory<HpsDBusClientImpl> weak_ptr_factory_{this};
};

}  // namespace

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
  // Do not create a new fake if it was initialized early in a test, to allow
  // the test to set its own client.
  if (!FakeHpsDBusClient::Get())
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
