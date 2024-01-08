// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/human_presence/human_presence_dbus_client.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/human_presence/fake_human_presence_dbus_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/hps/dbus-constants.h"

namespace ash {

namespace {

HumanPresenceDBusClient* g_instance = nullptr;

// Extracts result data out of a DBus response.
std::optional<hps::HpsResultProto> UnwrapHpsResult(dbus::Response* response) {
  if (response == nullptr) {
    return std::nullopt;
  }

  dbus::MessageReader reader(response);
  hps::HpsResultProto result;
  if (!reader.PopArrayOfBytesAsProto(&result)) {
    LOG(ERROR) << "Invalid DBus response data";
    return std::nullopt;
  }

  return result;
}

class HumanPresenceDBusClientImpl : public HumanPresenceDBusClient {
 public:
  explicit HumanPresenceDBusClientImpl(dbus::Bus* bus)
      : human_presence_proxy_(
            bus->GetObjectProxy(hps::kHpsServiceName,
                                dbus::ObjectPath(hps::kHpsServicePath))),
        weak_ptr_factory_(this) {
    // Connect to lock-on-leave changed signal.
    human_presence_proxy_->ConnectToSignal(
        hps::kHpsServiceInterface, hps::kHpsSenseChanged,
        base::BindRepeating(
            &HumanPresenceDBusClientImpl::HpsSenseChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&HumanPresenceDBusClientImpl::HpsSenseChangedConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Connect to snooping protection changed signal.
    human_presence_proxy_->ConnectToSignal(
        hps::kHpsServiceInterface, hps::kHpsNotifyChanged,
        base::BindRepeating(
            &HumanPresenceDBusClientImpl::HpsNotifyChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&HumanPresenceDBusClientImpl::HpsNotifyChangedConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor daemon restarts.
    human_presence_proxy_->SetNameOwnerChangedCallback(
        base::BindRepeating(&HumanPresenceDBusClientImpl::NameOwnerChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  ~HumanPresenceDBusClientImpl() override = default;

  HumanPresenceDBusClientImpl(const HumanPresenceDBusClientImpl&) = delete;
  HumanPresenceDBusClientImpl& operator=(const HumanPresenceDBusClientImpl&) =
      delete;

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
      observer.OnHpsSenseChanged(result);
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
      observer.OnHpsNotifyChanged(result);
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

  // HumanPresenceDBusClient:
  void GetResultHpsSense(GetResultCallback cb) override {
    dbus::MethodCall method_call(hps::kHpsServiceInterface,
                                 hps::kGetResultHpsSense);
    dbus::MessageWriter writer(&method_call);
    human_presence_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UnwrapHpsResult).Then(std::move(cb)));
  }

  void GetResultHpsNotify(GetResultCallback cb) override {
    dbus::MethodCall method_call(hps::kHpsServiceInterface,
                                 hps::kGetResultHpsNotify);
    dbus::MessageWriter writer(&method_call);
    human_presence_proxy_->CallMethod(
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
    EnableHumanPresenceFeature(hps::kEnableHpsSense, config);
  }

  void DisableHpsSense() override {
    DisableHumanPresenceFeature(hps::kDisableHpsSense);
  }

  void EnableHpsNotify(const hps::FeatureConfig& config) override {
    EnableHumanPresenceFeature(hps::kEnableHpsNotify, config);
  }

  void DisableHpsNotify() override {
    DisableHumanPresenceFeature(hps::kDisableHpsNotify);
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    human_presence_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

 private:
  // Send a method call to the human presence service with given method name and
  // config.
  void EnableHumanPresenceFeature(const std::string& method_name,
                                  const hps::FeatureConfig& config) {
    dbus::MethodCall method_call(hps::kHpsServiceInterface, method_name);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(config)) {
      LOG(ERROR) << "Failed to encode protobuf for " << method_name;
    } else {
      human_presence_proxy_->CallMethod(&method_call,
                                        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                        base::DoNothing());
    }
  }

  // Send a method call to HpsDBus with given method name.
  void DisableHumanPresenceFeature(const std::string& method_name) {
    dbus::MethodCall method_call(hps::kHpsServiceInterface, method_name);
    human_presence_proxy_->CallMethod(&method_call,
                                      dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                      base::DoNothing());
  }

  const raw_ptr<dbus::ObjectProxy> human_presence_proxy_;

  base::ObserverList<Observer> observers_;

  // Must be last class member.
  base::WeakPtrFactory<HumanPresenceDBusClientImpl> weak_ptr_factory_{this};
};

}  // namespace

HumanPresenceDBusClient::Observer::~Observer() = default;

HumanPresenceDBusClient::HumanPresenceDBusClient() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

HumanPresenceDBusClient::~HumanPresenceDBusClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
void HumanPresenceDBusClient::Initialize(dbus::Bus* bus) {
  DCHECK_NE(bus, nullptr);
  new HumanPresenceDBusClientImpl(bus);
}

// static
void HumanPresenceDBusClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a test, to allow
  // the test to set its own client.
  if (!FakeHumanPresenceDBusClient::Get())
    new FakeHumanPresenceDBusClient();
}

// static
void HumanPresenceDBusClient::Shutdown() {
  DCHECK_NE(g_instance, nullptr);
  delete g_instance;
}

// static
HumanPresenceDBusClient* HumanPresenceDBusClient::Get() {
  return g_instance;
}

}  // namespace ash
