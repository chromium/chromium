// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/install_attributes_client.h"

#include <memory>
#include <utility>

#include <google/protobuf/message_lite.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_install_attributes_client.h"
#include "chromeos/dbus/common/blocking_method_caller.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace ash {
namespace {

// The default timeout for all method call within InstallAttributes interface.
// Note that it is known that cryptohomed could be slow to respond to calls
// certain conditions. D-Bus call blocking for as long as 2 minutes have been
// observed in testing conditions/CQ.
constexpr int kInstallAttributesDefaultTimeoutMS = 5 * 60 * 1000;

InstallAttributesClient* g_instance = nullptr;

// Tries to parse a proto message from |response| into |proto|.
// Returns false if |response| is nullptr or the message cannot be parsed.
bool ParseProto(dbus::Response* response,
                google::protobuf::MessageLite* proto) {
  if (!response) {
    LOG(ERROR) << "Failed to call cryptohomed";
    return false;
  }

  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(proto)) {
    LOG(ERROR) << "Failed to parse response message from cryptohomed";
    return false;
  }

  return true;
}

// "Real" implementation of InstallAttributesClient talking to the cryptohomed's
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
        ::user_data_auth::kUserDataAuthServiceName,
        dbus::ObjectPath(::user_data_auth::kUserDataAuthServicePath));
    blocking_method_caller_ =
        std::make_unique<chromeos::BlockingMethodCaller>(bus, proxy_);
  }

  // InstallAttributesClient override:

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void InstallAttributesGet(
      const ::user_data_auth::InstallAttributesGetRequest& request,
      InstallAttributesGetCallback callback) override {
    CallProtoMethod(::user_data_auth::kInstallAttributesGet,
                    ::user_data_auth::kInstallAttributesInterface, request,
                    std::move(callback));
  }

  void InstallAttributesFinalize(
      const ::user_data_auth::InstallAttributesFinalizeRequest& request,
      InstallAttributesFinalizeCallback callback) override {
    CallProtoMethod(::user_data_auth::kInstallAttributesFinalize,
                    ::user_data_auth::kInstallAttributesInterface, request,
                    std::move(callback));
  }

  void InstallAttributesGetStatus(
      const ::user_data_auth::InstallAttributesGetStatusRequest& request,
      InstallAttributesGetStatusCallback callback) override {
    CallProtoMethod(::user_data_auth::kInstallAttributesGetStatus,
                    ::user_data_auth::kInstallAttributesInterface, request,
                    std::move(callback));
  }

  void RemoveFirmwareManagementParameters(
      const ::user_data_auth::RemoveFirmwareManagementParametersRequest&
          request,
      RemoveFirmwareManagementParametersCallback callback) override {
    CallProtoMethod(::user_data_auth::kRemoveFirmwareManagementParameters,
                    ::user_data_auth::kInstallAttributesInterface, request,
                    std::move(callback));
  }

  void SetFirmwareManagementParameters(
      const ::user_data_auth::SetFirmwareManagementParametersRequest& request,
      SetFirmwareManagementParametersCallback callback) override {
    CallProtoMethod(::user_data_auth::kSetFirmwareManagementParameters,
                    ::user_data_auth::kInstallAttributesInterface, request,
                    std::move(callback));
  }

  void GetFirmwareManagementParameters(
      const ::user_data_auth::GetFirmwareManagementParametersRequest& request,
      GetFirmwareManagementParametersCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetFirmwareManagementParameters,
                    ::user_data_auth::kInstallAttributesInterface, request,
                    std::move(callback));
  }

  absl::optional<::user_data_auth::InstallAttributesGetReply>
  BlockingInstallAttributesGet(
      const ::user_data_auth::InstallAttributesGetRequest& request) override {
    return BlockingCallProtoMethod<::user_data_auth::InstallAttributesGetReply>(
        ::user_data_auth::kInstallAttributesGet,
        ::user_data_auth::kInstallAttributesInterface, request);
  }

  absl::optional<::user_data_auth::InstallAttributesSetReply>
  BlockingInstallAttributesSet(
      const ::user_data_auth::InstallAttributesSetRequest& request) override {
    return BlockingCallProtoMethod<::user_data_auth::InstallAttributesSetReply>(
        ::user_data_auth::kInstallAttributesSet,
        ::user_data_auth::kInstallAttributesInterface, request);
  }

  absl::optional<::user_data_auth::InstallAttributesFinalizeReply>
  BlockingInstallAttributesFinalize(
      const ::user_data_auth::InstallAttributesFinalizeRequest& request)
      override {
    return BlockingCallProtoMethod<
        ::user_data_auth::InstallAttributesFinalizeReply>(
        ::user_data_auth::kInstallAttributesFinalize,
        ::user_data_auth::kInstallAttributesInterface, request);
  }

  absl::optional<::user_data_auth::InstallAttributesGetStatusReply>
  BlockingInstallAttributesGetStatus(
      const ::user_data_auth::InstallAttributesGetStatusRequest& request)
      override {
    return BlockingCallProtoMethod<
        ::user_data_auth::InstallAttributesGetStatusReply>(
        ::user_data_auth::kInstallAttributesGetStatus,
        ::user_data_auth::kInstallAttributesInterface, request);
  }

 private:
  // Calls cryptohomed's |method_name| method in |interface_name| interface,
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
          FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
      return;
    }
    // Bind with the weak pointer of |this| so the response is not
    // handled once |this| is already destroyed.
    proxy_->CallMethod(
        &method_call, timeout_ms,
        base::BindOnce(&InstallAttributesClientImpl::HandleResponse<ReplyType>,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Calls cryptohomed's |method_name| method in |interface_name| interface,
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
      std::move(callback).Run(absl::nullopt);
      return;
    }
    std::move(callback).Run(reply_proto);
  }

  template <typename ReplyType, typename RequestType>
  absl::optional<ReplyType> BlockingCallProtoMethod(
      const char* method_name,
      const char* interface_name,
      const RequestType& request) {
    dbus::MethodCall method_call(interface_name, method_name);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to append protobuf when calling InstallAttributes "
                    "method (blocking) "
                 << method_name;
      return absl::nullopt;
    }

    std::unique_ptr<dbus::Response> response(
        blocking_method_caller_->CallMethodAndBlock(&method_call));

    if (!response) {
      LOG(ERROR) << "DBus call failed for InstallAttributes method (blocking) "
                 << method_name;
      return absl::nullopt;
    }

    ReplyType reply_proto;
    if (!ParseProto(response.get(), &reply_proto)) {
      LOG(ERROR)
          << "Failed to parse proto from InstallAttributes method (blocking) "
          << method_name;
      return absl::nullopt;
    }

    return reply_proto;
  }

  // D-Bus proxy for cryptohomed, not owned.
  dbus::ObjectProxy* proxy_ = nullptr;

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
