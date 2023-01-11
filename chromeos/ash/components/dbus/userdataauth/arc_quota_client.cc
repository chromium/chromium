// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/arc_quota_client.h"

#include <utility>

#include <google/protobuf/message_lite.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_arc_quota_client.h"
#include "chromeos/dbus/common/blocking_method_caller.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace ash {
namespace {

// The default timeout for all method call within ArcQuota interface.
// Note that it is known that cryptohomed could be slow to respond to calls
// certain conditions. D-Bus call blocking for as long as 2 minutes have been
// observed in testing conditions/CQ.
constexpr int kArcQuotaDefaultTimeoutMS = 5 * 60 * 1000;

ArcQuotaClient* g_instance = nullptr;

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

// "Real" implementation of ArcQuotaClient talking to the cryptohomed's
// ArcQuota interface on the Chrome OS side.
class ArcQuotaClientImpl : public ArcQuotaClient {
 public:
  ArcQuotaClientImpl() = default;
  ~ArcQuotaClientImpl() override = default;

  // Not copyable or movable.
  ArcQuotaClientImpl(const ArcQuotaClientImpl&) = delete;
  ArcQuotaClientImpl& operator=(const ArcQuotaClientImpl&) = delete;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        ::user_data_auth::kUserDataAuthServiceName,
        dbus::ObjectPath(::user_data_auth::kUserDataAuthServicePath));
  }

  // ArcQuotaClient override:

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void GetArcDiskFeatures(
      const ::user_data_auth::GetArcDiskFeaturesRequest& request,
      GetArcDiskFeaturesCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetArcDiskFeatures,
                    ::user_data_auth::kArcQuotaInterface, request,
                    std::move(callback));
  }

  void GetCurrentSpaceForArcUid(
      const ::user_data_auth::GetCurrentSpaceForArcUidRequest& request,
      GetCurrentSpaceForArcUidCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetCurrentSpaceForArcUid,
                    ::user_data_auth::kArcQuotaInterface, request,
                    std::move(callback));
  }

  void GetCurrentSpaceForArcGid(
      const ::user_data_auth::GetCurrentSpaceForArcGidRequest& request,
      GetCurrentSpaceForArcGidCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetCurrentSpaceForArcGid,
                    ::user_data_auth::kArcQuotaInterface, request,
                    std::move(callback));
  }

  void GetCurrentSpaceForArcProjectId(
      const ::user_data_auth::GetCurrentSpaceForArcProjectIdRequest& request,
      GetCurrentSpaceForArcProjectIdCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetCurrentSpaceForArcProjectId,
                    ::user_data_auth::kArcQuotaInterface, request,
                    std::move(callback));
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
      LOG(ERROR) << "Failed to append protobuf when calling ArcQuota method "
                 << method_name;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
      return;
    }
    // Bind with the weak pointer of |this| so the response is not
    // handled once |this| is already destroyed.
    proxy_->CallMethod(
        &method_call, timeout_ms,
        base::BindOnce(&ArcQuotaClientImpl::HandleResponse<ReplyType>,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Calls cryptohomed's |method_name| method in |interface_name| interface,
  // passing in |request| as input with the default ArcQuota timeout.
  // Once the (asynchronous) call finishes, |callback| is called with the
  // response proto.
  template <typename RequestType, typename ReplyType>
  void CallProtoMethod(const char* method_name,
                       const char* interface_name,
                       const RequestType& request,
                       chromeos::DBusMethodCallback<ReplyType> callback) {
    CallProtoMethodWithTimeout(method_name, interface_name,
                               kArcQuotaDefaultTimeoutMS, request,
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
      LOG(ERROR) << "Failed to parse reply protobuf from ArcQuota method";
      std::move(callback).Run(absl::nullopt);
      return;
    }
    std::move(callback).Run(reply_proto);
  }

  // D-Bus proxy for cryptohomed, not owned.
  dbus::ObjectProxy* proxy_ = nullptr;

  base::WeakPtrFactory<ArcQuotaClientImpl> weak_factory_{this};
};

}  // namespace

ArcQuotaClient::ArcQuotaClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ArcQuotaClient::~ArcQuotaClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ArcQuotaClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ArcQuotaClientImpl())->Init(bus);
}

// static
void ArcQuotaClient::InitializeFake() {
  new FakeArcQuotaClient();
}

// static
void ArcQuotaClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
ArcQuotaClient* ArcQuotaClient::Get() {
  return g_instance;
}

}  // namespace ash
