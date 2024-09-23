// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/cec_service/cec_service_client.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/dbus/cec_service/fake_cec_service_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

CecServiceClient* g_instance = nullptr;

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
      NOTREACHED_IN_MIGRATION() << "Received unknown state " << power_state;
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

  CecServiceClientImpl(const CecServiceClientImpl&) = delete;
  CecServiceClientImpl& operator=(const CecServiceClientImpl&) = delete;

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

  void Init(dbus::Bus* bus) override {
    cec_service_proxy_ =
        bus->GetObjectProxy(cecservice::kCecServiceName,
                            dbus::ObjectPath(cecservice::kCecServicePath));
  }

 private:
  scoped_refptr<dbus::ObjectProxy> cec_service_proxy_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CecServiceClient

// static
CecServiceClient* CecServiceClient::Get() {
  return g_instance;
}

// static
void CecServiceClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new CecServiceClientImpl())->Init(bus);
}

// static
void CecServiceClient::InitializeFake() {
  (new FakeCecServiceClient())->Init(nullptr);
}

// static
void CecServiceClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

CecServiceClient::CecServiceClient() {
  CHECK(!g_instance);
  g_instance = this;
}

CecServiceClient::~CecServiceClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
