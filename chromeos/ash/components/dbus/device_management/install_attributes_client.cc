// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/device_management/install_attributes_client.h"

#include <memory>
#include <utility>

#include <google/protobuf/message_lite.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/device_management/fake_install_attributes_client.h"
#include "chromeos/dbus/common/blocking_method_caller.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/device_management/dbus-constants.h"

namespace ash {
namespace {

// The default timeout for all method call within InstallAttributes interface.
// Note that it is known that device_managementd could be slow to respond to calls
// in certain conditions. D-Bus call blocking for as long as 2 minutes have been
// observed in testing conditions/CQ.
constexpr int kInstallAttributesDefaultTimeoutMS = 5 * 60 * 1000;

InstallAttributesClient* g_instance = nullptr;

// Tries to parse a proto message from |response| into |proto|.
// Returns false if |response| is nullptr or the message cannot be parsed.
bool ParseProto(dbus::Response* response,
                google::protobuf::MessageLite* proto) {
  if (!response) {
    LOG(ERROR) << "Failed to call device_managementd";
    return false;
  }

  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(proto)) {
    LOG(ERROR) << "Failed to parse response message from device_managementd";
    return false;
  }

  return true;
}

// "Real" implementation of InstallAttributesClient talking to the device_managementd's
// InstallAttributes interface on the Chrome OS side.
class InstallAttributesClientImpl : public InstallAttributesClient {
 public:
  InstallAttributesClientImpl() = default;
  ~InstallAttributesClientImpl() override = default;

  // Not copyable or movable.
  InstallAttributesClientImpl(const InstallAttributesClientImpl&) = delete;
  InstallAttributesClientImpl& operator=(const InstallAttributesClientImpl&) =
      delete;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        ::device_management::kDeviceManagementServiceName,
        dbus::ObjectPath(::device_management::kDeviceManagementServicePath));
    blocking_method_caller_ =
        std::make_unique<chromeos::BlockingMethodCaller>(bus, proxy_);
  }

  // InstallAttributesClient override:

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void InstallAttributesGet(
      const ::device_management::InstallAttributesGetRequest& request,
      InstallAttributesGetCallback callback) override {
    CallProtoMethod(::device_management::kInstallAttributesGet,
                    ::device_management::kDeviceManagementInterface, request,
                    std::move(callback));
  }

  void InstallAttributesFinalize(
      const ::device_management::InstallAttributesFinalizeRequest& request,
      InstallAttributesFinalizeCallback callback) override {
    CallProtoMethod(::device_management::kInstallAttributesFinalize,
                    ::device_management::kDeviceManagementInterface, request,
                    std::move(callback));
  }

  void InstallAttributesGetStatus(
      const ::device_management::InstallAttributesGetStatusRequest& request,
      InstallAttributesGetStatusCallback callback) override {
    CallProtoMethod(::device_management::kInstallAttributesGetStatus,
                    ::device_management::kDeviceManagementInterface, request,
                    std::move(callback));
  }

  void RemoveFirmwareManagementParameters(
      const ::device_management::RemoveFirmwareManagementParametersRequest&
          request,
      RemoveFirmwareManagementParametersCallback callback) override {
    CallProtoMethod(::device_management::kRemoveFirmwareManagementParameters,
                    ::device_management::kDeviceManagementInterface, request,
                    std::move(callback));
  }

  void SetFirmwareManagementParameters(
      const ::device_management::SetFirmwareManagementParametersRequest& request,
      SetFirmwareManagementParametersCallback callback) override {
    CallProtoMethod(::device_management::kSetFirmwareManagementParameters,
                    ::device_management::kDeviceManagementInterface, request,
                    std::move(callback));
  }

  void GetFirmwareManagementParameters(
      const ::device_management::GetFirmwareManagementParametersRequest& request,
      GetFirmwareManagementParametersCallback callback) override {
    CallProtoMethod(::device_management::kGetFirmwareManagementParameters,
                    ::device_management::kDeviceManagementInterface, request,
                    std::move(callback));
  }

  std::optional<::device_management::InstallAttributesGetReply>
  BlockingInstallAttributesGet(
      const ::device_management::InstallAttributesGetRequest& request) override {
    return BlockingCallProtoMethod<::device_management::InstallAttributesGetReply>(
        ::device_management::kInstallAttributesGet,
        ::device_management::kDeviceManagementInterface, request);
  }

  std::optional<::device_management::InstallAttributesSetReply>
  BlockingInstallAttributesSet(
      const ::device_management::InstallAttributesSetRequest& request) override {
    return BlockingCallProtoMethod<::device_management::InstallAttributesSetReply>(
        ::device_management::kInstallAttributesSet,
        ::device_management::kDeviceManagementInterface, request);
  }

  std::optional<::device_management::InstallAttributesFinalizeReply>
  BlockingInstallAttributesFinalize(
      const ::device_management::InstallAttributesFinalizeRequest& request)
      override {
    return BlockingCallProtoMethod<
        ::device_management::InstallAttributesFinalizeReply>(
        ::device_management::kInstallAttributesFinalize,
        ::device_management::kDeviceManagementInterface, request);
  }

  std::optional<::device_management::InstallAttributesGetStatusReply>
  BlockingInstallAttributesGetStatus(
      const ::device_management::InstallAttributesGetStatusRequest& request)
      override {
    return BlockingCallProtoMethod<
        ::device_management::InstallAttributesGetStatusReply>(
        ::device_management::kInstallAttributesGetStatus,
        ::device_management::kDeviceManagementInterface, request);
  }

 private:
  // Calls device_managementd's |method_name| method in |interface_name| interface,
  // passing in |request| as input with |timeout_ms|. Once the (asynchronous)
  // call finishes, |callback| is called with the response proto.
  template <typename RequestType, typename ReplyType>
  void CallProtoMethodWithTimeout(
      const char* method_name,
      const char* interface_name,
      int timeout_ms,
      const RequestType& request,
      chromeos::DBusMethodCallback<ReplyType> callback) {
    dbus::MethodCall method_call(interface_name, method_name);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR)
          << "Failed to append protobuf when calling InstallAttributes method "
          << method_name;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    // Bind with the weak pointer of |this| so the response is not
    // handled once |this| is already destroyed.
    proxy_->CallMethod(
        &method_call, timeout_ms,
        base::BindOnce(&InstallAttributesClientImpl::HandleResponse<ReplyType>,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Calls device_managementd's |method_name| method in |interface_name| interface,
  // passing in |request| as input with the default InstallAttributes timeout.
  // Once the (asynchronous) call finishes, |callback| is called with the
  // response proto.
  template <typename RequestType, typename ReplyType>
  void CallProtoMethod(const char* method_name,
                       const char* interface_name,
                       const RequestType& request,
                       chromeos::DBusMethodCallback<ReplyType> callback) {
    CallProtoMethodWithTimeout(method_name, interface_name,
                               kInstallAttributesDefaultTimeoutMS, request,
                               std::move(callback));
  }

  // Parses the response proto message from |response| and calls |callback| with
  // the decoded message. Calls |callback| with std::nullopt on error, including
  // timeout.
  template <typename ReplyType>
  void HandleResponse(chromeos::DBusMethodCallback<ReplyType> callback,
                      dbus::Response* response) {
    ReplyType reply_proto;
    if (!ParseProto(response, &reply_proto)) {
      LOG(ERROR)
          << "Failed to parse reply protobuf from InstallAttributes method";
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(reply_proto);
  }

  template <typename ReplyType, typename RequestType>
  std::optional<ReplyType> BlockingCallProtoMethod(const char* method_name,
                                                   const char* interface_name,
                                                   const RequestType& request) {
    dbus::MethodCall method_call(interface_name, method_name);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to append protobuf when calling InstallAttributes "
                    "method (blocking) "
                 << method_name;
      return std::nullopt;
    }

    std::unique_ptr<dbus::Response> response(
        blocking_method_caller_->CallMethodAndBlock(&method_call)
            .value_or(nullptr));

    if (!response) {
      LOG(ERROR) << "DBus call failed for InstallAttributes method (blocking) "
                 << method_name;
      return std::nullopt;
    }

    ReplyType reply_proto;
    if (!ParseProto(response.get(), &reply_proto)) {
      LOG(ERROR)
          << "Failed to parse proto from InstallAttributes method (blocking) "
          << method_name;
      return std::nullopt;
    }

    return reply_proto;
  }

  // D-Bus proxy for device_managementd, not owned.
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // For making blocking dbus calls.
  std::unique_ptr<chromeos::BlockingMethodCaller> blocking_method_caller_;

  base::WeakPtrFactory<InstallAttributesClientImpl> weak_factory_{this};
};

}  // namespace

InstallAttributesClient::InstallAttributesClient() {
  CHECK(!g_instance);
  g_instance = this;
}

InstallAttributesClient::~InstallAttributesClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void InstallAttributesClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new InstallAttributesClientImpl())->Init(bus);
}

// static
void InstallAttributesClient::InitializeFake() {
  new FakeInstallAttributesClient();
}

// static
void InstallAttributesClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
InstallAttributesClient* InstallAttributesClient::Get() {
  return g_instance;
}

}  // namespace ash
