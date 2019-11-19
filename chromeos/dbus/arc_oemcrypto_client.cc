// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/arc_oemcrypto_client.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class ArcOemCryptoClientImpl : public ArcOemCryptoClient {
 public:
  ArcOemCryptoClientImpl() {}
  ~ArcOemCryptoClientImpl() override = default;

  // ArcOemCryptoClient override:
  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override {
    if (!service_available_) {
      DVLOG(1) << "ArcOemCrypto D-Bus service not available";
      std::move(callback).Run(false);
      return;
    }
    dbus::MethodCall method_call(arc_oemcrypto::kArcOemCryptoServiceInterface,
                                 arc_oemcrypto::kBootstrapMojoConnection);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(fd.get());
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcOemCryptoClientImpl::OnVoidDBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  // DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        arc_oemcrypto::kArcOemCryptoServiceName,
        dbus::ObjectPath(arc_oemcrypto::kArcOemCryptoServicePath));
    proxy_->WaitForServiceToBeAvailable(
        base::BindOnce(&ArcOemCryptoClientImpl::OnServiceAvailable,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Runs the callback with the method call result.
  void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                        dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  void OnServiceAvailable(bool service_is_available) {
    service_available_ = service_is_available;
  }

  dbus::ObjectProxy* proxy_ = nullptr;
  bool service_available_ = false;

  base::WeakPtrFactory<ArcOemCryptoClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcOemCryptoClientImpl);
};

}  // namespace

ArcOemCryptoClient::ArcOemCryptoClient() = default;

ArcOemCryptoClient::~ArcOemCryptoClient() = default;

// static
std::unique_ptr<ArcOemCryptoClient> ArcOemCryptoClient::Create() {
  return std::make_unique<ArcOemCryptoClientImpl>();
}

}  // namespace chromeos
