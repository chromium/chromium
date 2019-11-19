// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cec_service_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Translates a power state from a D-Bus response to the types exposed by
// CecServiceClient.
CecServiceClient::PowerState ConvertDBusPowerState(int32_t power_state) {
  switch (power_state) {
    case cecservice::kTvPowerStatusError:
      return CecServiceClient::PowerState::kError;
    case cecservice::kTvPowerStatusAdapterNotConfigured:
      return CecServiceClient::PowerState::kAdapterNotConfigured;
    case cecservice::kTvPowerStatusNoTv:
      return CecServiceClient::PowerState::kNoDevice;
    case cecservice::kTvPowerStatusOn:
      return CecServiceClient::PowerState::kOn;
    case cecservice::kTvPowerStatusStandBy:
      return CecServiceClient::PowerState::kStandBy;
    case cecservice::kTvPowerStatusToOn:
      return CecServiceClient::PowerState::kTransitioningToOn;
    case cecservice::kTvPowerStatusToStandBy:
      return CecServiceClient::PowerState::kTransitioningToStandBy;
    case cecservice::kTvPowerStatusUnknown:
      return CecServiceClient::PowerState::kUnknown;
    default:
      NOTREACHED() << "Received unknown state " << power_state;
      return CecServiceClient::PowerState::kUnknown;
  }
}

void OnGetTvsPowerStatus(CecServiceClient::PowerStateCallback callback,
                         dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "QueryDisplayCecPowerState call failed, no response";
    std::move(callback).Run({});
    return;
  }

  dbus::MessageReader reader(response);

  dbus::MessageReader array_reader(nullptr);
  if (!reader.PopArray(&array_reader)) {
    LOG(ERROR) << "Unable to read back array for QueryDisplayCecPowerState";
    std::move(callback).Run({});
    return;
  }

  std::vector<CecServiceClient::PowerState> result;
  while (array_reader.HasMoreData()) {
    int32_t low_level_power_state = -1;
    if (!array_reader.PopInt32(&low_level_power_state)) {
      LOG(ERROR) << "Unable to pop state for QueryDisplayCecPowerState";
      std::move(callback).Run({});
      return;
    }

    result.push_back(ConvertDBusPowerState(low_level_power_state));
  }

  std::move(callback).Run(result);
}

// Real implementation of CecServiceClient.
class CecServiceClientImpl : public CecServiceClient {
 public:
  CecServiceClientImpl() = default;

  ~CecServiceClientImpl() override = default;

  void SendStandBy() override {
    dbus::MethodCall method_call(cecservice::kCecServiceInterface,
                                 cecservice::kSendStandByToAllDevicesMethod);
    cec_service_proxy_->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   base::DoNothing());
  }

  void SendWakeUp() override {
    dbus::MethodCall method_call(cecservice::kCecServiceInterface,
                                 cecservice::kSendWakeUpToAllDevicesMethod);
    cec_service_proxy_->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   base::DoNothing());
  }

  void QueryDisplayCecPowerState(PowerStateCallback callback) override {
    dbus::MethodCall query_method(cecservice::kCecServiceInterface,
                                  cecservice::kGetTvsPowerStatus);
    cec_service_proxy_->CallMethod(
        &query_method, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnGetTvsPowerStatus, std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    cec_service_proxy_ =
        bus->GetObjectProxy(cecservice::kCecServiceName,
                            dbus::ObjectPath(cecservice::kCecServicePath));
  }

 private:
  scoped_refptr<dbus::ObjectProxy> cec_service_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CecServiceClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CecServiceClient

CecServiceClient::CecServiceClient() = default;

CecServiceClient::~CecServiceClient() = default;

// static
std::unique_ptr<CecServiceClient> CecServiceClient::Create() {
  return std::make_unique<CecServiceClientImpl>();
}

}  // namespace chromeos
