// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/private_computing/private_computing_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/private_computing/fake_private_computing_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/private_computing/dbus-constants.h"

namespace ash {
namespace {

PrivateComputingClient* g_instance = nullptr;

const char kDbusCallFailure[] = "Failed to call private computing.";
const char kProtoMessageParsingFailure[] =
    "Failed to parse response message from private computing.";

// Tries to parse a proto message from |response| into |proto| and returns null
// if successful. If |response| is nullptr or the message cannot be parsed it
// will return an appropriate error message.
const char* DeserializeProto(dbus::Response* response,
                             google::protobuf::MessageLite* proto) {
  if (!response)
    return kDbusCallFailure;

  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(proto))
    return kProtoMessageParsingFailure;

  return nullptr;
}

// "Real" implementation of PrivateComputingClient talking to the
// PrivateComputing daemon on the Chrome OS side.
class PrivateComputingClientImpl : public PrivateComputingClient {
 public:
  PrivateComputingClientImpl() = default;

  PrivateComputingClientImpl(const PrivateComputingClientImpl&) = delete;
  PrivateComputingClientImpl& operator=(const PrivateComputingClientImpl&) =
      delete;

  ~PrivateComputingClientImpl() override = default;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        private_computing::kPrivateComputingServiceName,
        dbus::ObjectPath(private_computing::kPrivateComputingServicePath));
  }

  // PrivateComputingClient:
  void SaveLastPingDatesStatus(
      const private_computing::SaveStatusRequest& request,
      SaveStatusCallback callback) override {
    dbus::MethodCall method_call(private_computing::kPrivateComputingInterface,
                                 private_computing::kSaveLastPingDatesStatus);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      private_computing::SaveStatusResponse response;
      response.set_error_message(
          base::StrCat({"Failure to call d-bus method: ",
                        private_computing::kSaveLastPingDatesStatus}));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), response));
      return;
    }

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &PrivateComputingClientImpl::HandleSaveLastPingDatesStatusResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetLastPingDatesStatus(GetStatusCallback callback) override {
    dbus::MethodCall method_call(private_computing::kPrivateComputingInterface,
                                 private_computing::kGetLastPingDatesStatus);
    dbus::MessageWriter writer(&method_call);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &PrivateComputingClientImpl::HandleGetLastPingDatesStatusResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  TestInterface* GetTestInterface() override { return nullptr; }

 private:
  // Handle a DBus response from the private computing chromeos daemon,
  // invoking the callback that the method was originally called with the
  // success response.
  void HandleSaveLastPingDatesStatusResponse(SaveStatusCallback callback,
                                             dbus::Response* response) {
    private_computing::SaveStatusResponse response_proto;
    const char* error_message = DeserializeProto(response, &response_proto);
    if (error_message) {
      LOG(ERROR)
          << "Response from SaveLastPingDatesStatus contains error message "
          << error_message;
      response_proto.set_error_message(error_message);
    }

    std::move(callback).Run(response_proto);
  }

  // Handle a DBus response from the private computing chromeos daemon,
  // invoking the callback that the method was originally called with the
  // success response.
  void HandleGetLastPingDatesStatusResponse(GetStatusCallback callback,
                                            dbus::Response* response) {
    private_computing::GetStatusResponse response_proto;
    const char* error_message = DeserializeProto(response, &response_proto);
    if (error_message) {
      LOG(ERROR)
          << "Response from GetLastPingDatesStatus contains error message "
          << error_message;
      response_proto.set_error_message(error_message);
    }

    std::move(callback).Run(response_proto);
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // Note: This should remain the last member so that it will be destroyed
  // first, invalidating its weak pointers, before the other members are
  // destroyed.
  base::WeakPtrFactory<PrivateComputingClientImpl> weak_ptr_factory_{this};
};

}  // namespace

PrivateComputingClient::PrivateComputingClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

PrivateComputingClient::~PrivateComputingClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void PrivateComputingClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new PrivateComputingClientImpl())->Init(bus);
}

// static
void PrivateComputingClient::InitializeFake() {
  new FakePrivateComputingClient();
}

// static
void PrivateComputingClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
PrivateComputingClient* PrivateComputingClient::Get() {
  return g_instance;
}

}  // namespace ash
