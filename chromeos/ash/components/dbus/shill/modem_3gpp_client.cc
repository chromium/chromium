// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/modem_3gpp_client.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_modem_3gpp_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

Modem3gppClient* g_instance = nullptr;

// A class which makes method calls for 3gpp services via the
// org.freedesktop.ModemManager1.3gpp object.
class Modem3gppProxy {
 public:
  Modem3gppProxy(dbus::Bus* bus,
                 const std::string& service_name,
                 const dbus::ObjectPath& object_path)
      : proxy_(bus->GetObjectProxy(service_name, object_path)),
        service_name_(service_name) {}

  Modem3gppProxy(const Modem3gppProxy&) = delete;
  Modem3gppProxy& operator=(const Modem3gppProxy&) = delete;

  virtual ~Modem3gppProxy() = default;

  // Calls SetCarrierLock method.
  void SetCarrierLock(const std::string& config,
                      Modem3gppClient::CarrierLockCallback callback) {
    dbus::MethodCall method_call(modemmanager::kModemManager13gppInterface,
                                 modemmanager::kModem3gppSetCarrierLock);

    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfBytes(base::as_byte_span(config));

    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&Modem3gppProxy::OnSetCarrierLock,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  // Handles responses of SetCarrierLock method calls.
  void OnSetCarrierLock(Modem3gppClient::CarrierLockCallback callback,
                        dbus::Response* response,
                        dbus::ErrorResponse* error_response) {
    if (response &&
        response->GetMessageType() == dbus::Message::MESSAGE_METHOD_RETURN) {
      std::move(callback).Run(CarrierLockResult::kSuccess);
      return;
    }

    if (!error_response) {
      std::move(callback).Run(CarrierLockResult::kUnknownError);
      return;
    }

    std::string error_name;
    error_name = error_response->GetErrorName();
    if (error_name.ends_with("InvalidSignature")) {
      std::move(callback).Run(CarrierLockResult::kInvalidSignature);
    } else if (error_name.ends_with("InvalidImei")) {
      std::move(callback).Run(CarrierLockResult::kInvalidImei);
    } else if (error_name.ends_with("InvalidTimestamp")) {
      std::move(callback).Run(CarrierLockResult::kInvalidTimeStamp);
    } else if (error_name.ends_with("NetworkListTooLarge")) {
      std::move(callback).Run(CarrierLockResult::kNetworkListTooLarge);
    } else if (error_name.ends_with("AlgorithmNotSupported")) {
      std::move(callback).Run(CarrierLockResult::kAlgorithmNotSupported);
    } else if (error_name.ends_with("FeatureNotSupported")) {
      std::move(callback).Run(CarrierLockResult::kFeatureNotSupported);
    } else if (error_name.ends_with("DecodeOrParsingError")) {
      std::move(callback).Run(CarrierLockResult::kDecodeOrParsingError);
    } else if (error_name.ends_with("Unsupported")) {
      std::move(callback).Run(CarrierLockResult::kOperationNotSupported);
    } else {
      std::move(callback).Run(CarrierLockResult::kUnknownError);
    }
  }

  raw_ptr<dbus::ObjectProxy> proxy_;
  std::string service_name_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<Modem3gppProxy> weak_ptr_factory_{this};
};

class COMPONENT_EXPORT(CHROMEOS_DBUS) Modem3gppClientImpl
    : public Modem3gppClient {
 public:
  explicit Modem3gppClientImpl(dbus::Bus* bus) : bus_(bus) {}

  Modem3gppClientImpl(const Modem3gppClientImpl&) = delete;
  Modem3gppClientImpl& operator=(const Modem3gppClientImpl&) = delete;

  ~Modem3gppClientImpl() override = default;

  void SetCarrierLock(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      const std::string& config,
                      CarrierLockCallback callback) override {
    GetProxy(service_name, object_path)
        ->SetCarrierLock(config, std::move(callback));
  }

 private:
  // Map of 3gppProxies where Key is a pair of service name and object path
  using ProxyMap = std::map<std::pair<std::string, std::string>,
                            std::unique_ptr<Modem3gppProxy>>;

  // Returns a 3gppProxy for the given service name and object path.
  Modem3gppProxy* GetProxy(const std::string& service_name,
                           const dbus::ObjectPath& object_path) {
    const ProxyMap::key_type key(service_name, object_path.value());
    ProxyMap::const_iterator it = proxies_.find(key);
    if (it != proxies_.end()) {
      return it->second.get();
    }

    // There is no proxy for the service_name and object_path, create it.
    std::unique_ptr<Modem3gppProxy> proxy =
        std::make_unique<Modem3gppProxy>(bus_, service_name, object_path);
    Modem3gppProxy* proxy_ptr = proxy.get();
    proxies_[key] = std::move(proxy);
    return proxy_ptr;
  }

  raw_ptr<dbus::Bus> bus_;
  ProxyMap proxies_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Modem3gppClient

Modem3gppClient::Modem3gppClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

Modem3gppClient::~Modem3gppClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void Modem3gppClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  DCHECK(!g_instance);
  new Modem3gppClientImpl(bus);
}

// static
void Modem3gppClient::InitializeFake() {
  new FakeModem3gppClient();
}

// static
void Modem3gppClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
Modem3gppClient* Modem3gppClient::Get() {
  return g_instance;
}

}  // namespace ash
