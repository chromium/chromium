// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/modem_messaging_client.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_modem_messaging_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ModemMessagingClient* g_instance = nullptr;

// A class which makes method calls for SMS services via the
// org.freedesktop.ModemManager1.Messaging object.
class ModemMessagingProxy {
 public:
  typedef ModemMessagingClient::SmsReceivedHandler SmsReceivedHandler;
  using ListCallback = ModemMessagingClient::ListCallback;

  ModemMessagingProxy(dbus::Bus* bus,
                      const std::string& service_name,
                      const dbus::ObjectPath& object_path)
      : proxy_(bus->GetObjectProxy(service_name, object_path)),
        service_name_(service_name) {
    proxy_->ConnectToSignal(
        modemmanager::kModemManager1MessagingInterface,
        modemmanager::kSMSAddedSignal,
        base::BindRepeating(&ModemMessagingProxy::OnSmsAdded,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ModemMessagingProxy::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  ModemMessagingProxy(const ModemMessagingProxy&) = delete;
  ModemMessagingProxy& operator=(const ModemMessagingProxy&) = delete;

  virtual ~ModemMessagingProxy() = default;

  // Sets SmsReceived signal handler.
  void SetSmsReceivedHandler(const SmsReceivedHandler& handler) {
    DCHECK(sms_received_handler_.is_null());
    sms_received_handler_ = handler;
  }

  // Resets SmsReceived signal handler.
  void ResetSmsReceivedHandler() { sms_received_handler_.Reset(); }

  // Calls Delete method.
  void Delete(const dbus::ObjectPath& message_path,
              chromeos::VoidDBusMethodCallback callback) {
    dbus::MethodCall method_call(modemmanager::kModemManager1MessagingInterface,
                                 modemmanager::kSMSDeleteFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(message_path);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ModemMessagingProxy::OnDelete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Calls List method.
  virtual void List(ListCallback callback) {
    dbus::MethodCall method_call(modemmanager::kModemManager1MessagingInterface,
                                 modemmanager::kSMSListFunction);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ModemMessagingProxy::OnList,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  // Handles SmsAdded signal.
  void OnSmsAdded(dbus::Signal* signal) {
    dbus::ObjectPath message_path;
    bool complete = false;
    dbus::MessageReader reader(signal);
    if (!reader.PopObjectPath(&message_path) || !reader.PopBool(&complete)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (!sms_received_handler_.is_null()) {
      sms_received_handler_.Run(message_path, complete);
    }
  }

  // Handles responses of Delete method calls.
  void OnDelete(chromeos::VoidDBusMethodCallback callback,
                dbus::Response* response) {
    std::move(callback).Run(response);
  }

  // Handles responses of List method calls.
  void OnList(ListCallback callback, dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    std::vector<dbus::ObjectPath> sms_paths;
    if (!reader.PopArrayOfObjectPaths(&sms_paths)) {
      LOG(WARNING) << "Invalid response: " << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(std::move(sms_paths));
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded)
        << "Connect to " << interface << " " << signal << " failed.";
  }

  raw_ptr<dbus::ObjectProxy> proxy_;
  std::string service_name_;
  SmsReceivedHandler sms_received_handler_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ModemMessagingProxy> weak_ptr_factory_{this};
};

class COMPONENT_EXPORT(CHROMEOS_DBUS) ModemMessagingClientImpl
    : public ModemMessagingClient {
 public:
  explicit ModemMessagingClientImpl(dbus::Bus* bus) : bus_(bus) {}

  ModemMessagingClientImpl(const ModemMessagingClientImpl&) = delete;
  ModemMessagingClientImpl& operator=(const ModemMessagingClientImpl&) = delete;

  ~ModemMessagingClientImpl() override = default;

  void SetSmsReceivedHandler(const std::string& service_name,
                             const dbus::ObjectPath& object_path,
                             const SmsReceivedHandler& handler) override {
    GetProxy(service_name, object_path)->SetSmsReceivedHandler(handler);
  }

  void ResetSmsReceivedHandler(const std::string& service_name,
                               const dbus::ObjectPath& object_path) override {
    GetProxy(service_name, object_path)->ResetSmsReceivedHandler();
  }

  void Delete(const std::string& service_name,
              const dbus::ObjectPath& object_path,
              const dbus::ObjectPath& sms_path,
              chromeos::VoidDBusMethodCallback callback) override {
    GetProxy(service_name, object_path)->Delete(sms_path, std::move(callback));
  }

  void List(const std::string& service_name,
            const dbus::ObjectPath& object_path,
            ListCallback callback) override {
    GetProxy(service_name, object_path)->List(std::move(callback));
  }

  TestInterface* GetTestInterface() override { return nullptr; }

 private:
  using ProxyMap = std::map<std::pair<std::string, std::string>,
                            std::unique_ptr<ModemMessagingProxy>>;

  // Returns a SMSProxy for the given service name and object path.
  ModemMessagingProxy* GetProxy(const std::string& service_name,
                                const dbus::ObjectPath& object_path) {
    const ProxyMap::key_type key(service_name, object_path.value());
    ProxyMap::const_iterator it = proxies_.find(key);
    if (it != proxies_.end())
      return it->second.get();

    // There is no proxy for the service_name and object_path, create it.
    std::unique_ptr<ModemMessagingProxy> proxy(
        new ModemMessagingProxy(bus_, service_name, object_path));
    ModemMessagingProxy* proxy_ptr = proxy.get();
    proxies_[key] = std::move(proxy);
    return proxy_ptr;
  }

  raw_ptr<dbus::Bus> bus_;
  ProxyMap proxies_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ModemMessagingClient

ModemMessagingClient::ModemMessagingClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ModemMessagingClient::~ModemMessagingClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ModemMessagingClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new ModemMessagingClientImpl(bus);
}

// static
void ModemMessagingClient::InitializeFake() {
  new FakeModemMessagingClient();
}

// static
void ModemMessagingClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ModemMessagingClient* ModemMessagingClient::Get() {
  return g_instance;
}

}  // namespace ash
