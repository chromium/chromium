// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/rmad/rmad_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/rmad/fake_rmad_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {
RmadClient* g_instance = nullptr;
}  // namespace

class RmadClientImpl : public RmadClient {
 public:
  void Init(dbus::Bus* bus);

  void GetCurrentState(
      DBusMethodCallback<rmad::GetStateReply> callback) override;
  void TransitionNextState(
      const rmad::RmadState& state,
      DBusMethodCallback<rmad::GetStateReply> callback) override;
  void TransitionPreviousState(
      DBusMethodCallback<rmad::GetStateReply> callback) override;

  void AbortRma(DBusMethodCallback<rmad::AbortRmaReply> callback) override;

  RmadClientImpl() = default;
  RmadClientImpl(const RmadClientImpl&) = delete;
  RmadClientImpl& operator=(const RmadClientImpl&) = delete;
  ~RmadClientImpl() override = default;

 private:
  template <class T>
  void OnProtoReply(DBusMethodCallback<T> callback, dbus::Response* response);

  dbus::ObjectProxy* rmad_proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RmadClientImpl> weak_ptr_factory_{this};
};

void RmadClientImpl::Init(dbus::Bus* bus) {
  rmad_proxy_ = bus->GetObjectProxy(rmad::kRmadServiceName,
                                    dbus::ObjectPath(rmad::kRmadServicePath));
}

void RmadClientImpl::GetCurrentState(
    DBusMethodCallback<rmad::GetStateReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kGetCurrentStateMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::GetStateReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::TransitionNextState(
    const rmad::RmadState& state,
    DBusMethodCallback<rmad::GetStateReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kTransitionNextStateMethod);
  dbus::MessageWriter writer(&method_call);
  // Create the empty request proto.
  rmad::TransitionNextStateRequest protobuf_request;
  protobuf_request.set_allocated_state(new rmad::RmadState(state));
  if (!writer.AppendProtoAsArrayOfBytes(protobuf_request)) {
    LOG(ERROR) << "Error constructing message for "
               << rmad::kTransitionNextStateMethod;
    std::move(callback).Run(absl::nullopt);
    return;
  }
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::GetStateReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}
void RmadClientImpl::TransitionPreviousState(
    DBusMethodCallback<rmad::GetStateReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kTransitionPreviousStateMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::GetStateReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::AbortRma(
    DBusMethodCallback<rmad::AbortRmaReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName, rmad::kAbortRmaMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::AbortRmaReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

template <class T>
void RmadClientImpl::OnProtoReply(DBusMethodCallback<T> callback,
                                  dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "Error calling rmad function";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  dbus::MessageReader reader(response);
  T response_proto;
  if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
    LOG(ERROR) << "Unable to decode response for " << response->GetMember();
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // TODO(gavindodd): Does this need std::move()?
  std::move(callback).Run(response_proto);
}

RmadClient::RmadClient() {
  CHECK(!g_instance);
  g_instance = this;
}

RmadClient::~RmadClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void RmadClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new RmadClientImpl())->Init(bus);
}

// static
void RmadClient::InitializeFake() {
  new FakeRmadClient();
}

// static
void RmadClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
RmadClient* RmadClient::Get() {
  return g_instance;
}

}  // namespace chromeos
