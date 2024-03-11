// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/oobe_config/fake_oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_metrics.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/oobe_config/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

OobeConfigurationClient* g_instance = nullptr;

// The OobeConfigurationClient used in production.

class OobeConfigurationClientImpl : public OobeConfigurationClient {
 public:
  OobeConfigurationClientImpl() {}

  OobeConfigurationClientImpl(const OobeConfigurationClientImpl&) = delete;
  OobeConfigurationClientImpl& operator=(const OobeConfigurationClientImpl&) =
      delete;

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

  void DeleteFlexOobeConfig() override {
    dbus::MethodCall method_call(oobe_config::kOobeConfigRestoreInterface,
                                 oobe_config::kDeleteFlexOobeConfigMethod);
    proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OobeConfigurationClientImpl::OnDeleteFlexOobeConfig,
                       weak_ptr_factory_.GetWeakPtr()));
  }

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

  void OnDeleteFlexOobeConfig(dbus::Response* response,
                              dbus::ErrorResponse* error_response) {
    if (response &&
        response->GetMessageType() == dbus::Message::MESSAGE_METHOD_RETURN) {
      VLOG(0) << "Successfully deleted Flex OOBE config";
      RecordDeleteFlexOobeConfigDBusResult(
          DeleteFlexOobeConfigDBusResult::kSuccess);
      return;
    }

    if (!error_response) {
      LOG(ERROR)
          << "No error response for unsuccessful DeleteFlexOobeConfig call";
      RecordDeleteFlexOobeConfigDBusResult(
          DeleteFlexOobeConfigDBusResult::kErrorUnknown);
      return;
    }
    std::string error_code = error_response->GetErrorName();

    DeleteFlexOobeConfigDBusResult result =
        ConvertDeleteFlexOobeConfigDBusError(error_code);
    LOG(ERROR) << "DeleteFlexOobeConfig DBus result: "
               << static_cast<int>(result);
    RecordDeleteFlexOobeConfigDBusResult(result);
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<OobeConfigurationClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
OobeConfigurationClient* OobeConfigurationClient::Get() {
  return g_instance;
}

// static
void OobeConfigurationClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new OobeConfigurationClientImpl())->Init(bus);
}

// static
void OobeConfigurationClient::InitializeFake() {
  (new FakeOobeConfigurationClient())->Init(nullptr);
}

// static
void OobeConfigurationClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

OobeConfigurationClient::OobeConfigurationClient() {
  CHECK(!g_instance);
  g_instance = this;
}

OobeConfigurationClient::~OobeConfigurationClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
