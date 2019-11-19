// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/oobe_configuration_client.h"

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chromeos/dbus/oobe_config/oobe_config.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The OobeConfigurationClient used in production.

class OobeConfigurationClientImpl : public OobeConfigurationClient {
 public:
  OobeConfigurationClientImpl() {}

  ~OobeConfigurationClientImpl() override = default;

  // OobeConfigurationClient override:
  void CheckForOobeConfiguration(ConfigurationCallback callback) override {
    dbus::MethodCall method_call(
        oobe_config::kOobeConfigRestoreInterface,
        oobe_config::kProcessAndGetOobeAutoConfigMethod);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OobeConfigurationClientImpl::OnData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        oobe_config::kOobeConfigRestoreServiceName,
        dbus::ObjectPath(oobe_config::kOobeConfigRestoreServicePath));
  }

 private:
  void OnData(ConfigurationCallback callback, dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(false, std::string());
      return;
    }
    oobe_config::OobeRestoreData response_proto;
    dbus::MessageReader reader(response);
    int error_code;
    if (!reader.PopInt32(&error_code)) {
      LOG(ERROR) << "Failed to parse error code from DBus Response.";
      std::move(callback).Run(false, std::string());
      return;
    }
    if (error_code) {
      LOG(ERROR) << "Error during DBus call " << error_code;
      std::move(callback).Run(false, std::string());
      return;
    }
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Response.";
      std::move(callback).Run(false, std::string());
      return;
    }
    VLOG(0) << "Got oobe config, size = "
            << response_proto.chrome_config_json().size();
    if (response_proto.chrome_config_json().empty()) {
      std::move(callback).Run(false, std::string());
      return;
    }
    std::move(callback).Run(true, response_proto.chrome_config_json());
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<OobeConfigurationClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OobeConfigurationClientImpl);
};

// static
std::unique_ptr<OobeConfigurationClient> OobeConfigurationClient::Create() {
  return std::make_unique<OobeConfigurationClientImpl>();
}

}  // namespace chromeos
