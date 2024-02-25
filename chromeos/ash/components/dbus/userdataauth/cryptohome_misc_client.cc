// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"

#include <memory>
#include <utility>

#include <google/protobuf/message_lite.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/dbus/common/blocking_method_caller.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace ash {
namespace {

// The default timeout for all method call within CryptohomeMisc interface.
// Note that it is known that cryptohomed could be slow to respond to calls
// certain conditions. D-Bus call blocking for as long as 2 minutes have been
// observed in testing conditions/CQ.
constexpr int kCryptohomeMiscDefaultTimeoutMS = 5 * 60 * 1000;

CryptohomeMiscClient* g_instance = nullptr;

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

// "Real" implementation of CryptohomeMiscClient talking to the cryptohomed's
// CryptohomeMisc interface on the Chrome OS side.
class CryptohomeMiscClientImpl : public CryptohomeMiscClient {
 public:
  CryptohomeMiscClientImpl() = default;
  ~CryptohomeMiscClientImpl() override = default;

  // Not copyable or movable.
  CryptohomeMiscClientImpl(const CryptohomeMiscClientImpl&) = delete;
  CryptohomeMiscClientImpl& operator=(const CryptohomeMiscClientImpl&) = delete;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        ::user_data_auth::kUserDataAuthServiceName,
        dbus::ObjectPath(::user_data_auth::kUserDataAuthServicePath));
    blocking_method_caller_ =
        std::make_unique<chromeos::BlockingMethodCaller>(bus, proxy_);
  }

  // CryptohomeMiscClient override:

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void GetSystemSalt(const ::user_data_auth::GetSystemSaltRequest& request,
                     GetSystemSaltCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetSystemSalt,
                    ::user_data_auth::kCryptohomeMiscInterface, request,
                    std::move(callback));
  }

  void GetSanitizedUsername(
      const ::user_data_auth::GetSanitizedUsernameRequest& request,
      GetSanitizedUsernameCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetSanitizedUsername,
                    ::user_data_auth::kCryptohomeMiscInterface, request,
                    std::move(callback));
  }

  void GetLoginStatus(const ::user_data_auth::GetLoginStatusRequest& request,
                      GetLoginStatusCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetLoginStatus,
                    ::user_data_auth::kCryptohomeMiscInterface, request,
                    std::move(callback));
  }

  void LockToSingleUserMountUntilReboot(
      const ::user_data_auth::LockToSingleUserMountUntilRebootRequest& request,
      LockToSingleUserMountUntilRebootCallback callback) override {
    CallProtoMethod(::user_data_auth::kLockToSingleUserMountUntilReboot,
                    ::user_data_auth::kCryptohomeMiscInterface, request,
                    std::move(callback));
  }

  void GetRsuDeviceId(const ::user_data_auth::GetRsuDeviceIdRequest& request,
                      GetRsuDeviceIdCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetRsuDeviceId,
                    ::user_data_auth::kCryptohomeMiscInterface, request,
                    std::move(callback));
  }

  std::optional<::user_data_auth::GetSanitizedUsernameReply>
  BlockingGetSanitizedUsername(
      const ::user_data_auth::GetSanitizedUsernameRequest& request) override {
    return BlockingCallProtoMethod<::user_data_auth::GetSanitizedUsernameReply>(
        ::user_data_auth::kGetSanitizedUsername,
        ::user_data_auth::kCryptohomeMiscInterface, request);
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
          << "Failed to append protobuf when calling CryptohomeMisc method "
          << method_name;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    // Bind with the weak pointer of |this| so the response is not
    // handled once |this| is already destroyed.
    proxy_->CallMethod(
        &method_call, timeout_ms,
        base::BindOnce(&CryptohomeMiscClientImpl::HandleResponse<ReplyType>,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Calls cryptohomed's |method_name| method in |interface_name| interface,
  // passing in |request| as input with the default CryptohomeMisc timeout. Once
  // the (asynchronous) call finishes, |callback| is called with the response
  // proto.
  template <typename RequestType, typename ReplyType>
  void CallProtoMethod(const char* method_name,
                       const char* interface_name,
                       const RequestType& request,
                       chromeos::DBusMethodCallback<ReplyType> callback) {
    CallProtoMethodWithTimeout(method_name, interface_name,
                               kCryptohomeMiscDefaultTimeoutMS, request,
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
      LOG(ERROR) << "Failed to parse reply protobuf from CryptohomeMisc method";
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
      LOG(ERROR) << "Failed to append protobuf when calling CryptohomeMisc "
                    "method (blocking) "
                 << method_name;
      return std::nullopt;
    }

    std::unique_ptr<dbus::Response> response(
        blocking_method_caller_->CallMethodAndBlock(&method_call)
            .value_or(nullptr));

    if (!response) {
      LOG(ERROR) << "DBus call failed for CryptohomeMisc method (blocking) "
                 << method_name;
      return std::nullopt;
    }

    ReplyType reply_proto;
    if (!ParseProto(response.get(), &reply_proto)) {
      LOG(ERROR)
          << "Failed to parse proto from CryptohomeMisc method (blocking) "
          << method_name;
      return std::nullopt;
    }

    return reply_proto;
  }

  // D-Bus proxy for cryptohomed, not owned.
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // For making blocking dbus calls.
  std::unique_ptr<chromeos::BlockingMethodCaller> blocking_method_caller_;

  base::WeakPtrFactory<CryptohomeMiscClientImpl> weak_factory_{this};
};

}  // namespace

CryptohomeMiscClient::CryptohomeMiscClient() {
  CHECK(!g_instance);
  g_instance = this;
}

CryptohomeMiscClient::~CryptohomeMiscClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void CryptohomeMiscClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new CryptohomeMiscClientImpl())->Init(bus);
}

// static
void CryptohomeMiscClient::InitializeFake() {
  new FakeCryptohomeMiscClient();
}

// static
void CryptohomeMiscClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
CryptohomeMiscClient* CryptohomeMiscClient::Get() {
  return g_instance;
}

}  // namespace ash
