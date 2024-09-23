// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/regmon/regmon_client.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/dbus/regmon/fake_regmon_client.h"
#include "chromeos/dbus/regmon/regmon_service.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/regmon/dbus-constants.h"

namespace chromeos {
namespace {

RegmonClient* g_instance = nullptr;

const char kNoResponseFailure[] = "No response message received from regmon.";
const char kProtoMessageParsingFailure[] =
    "Failed to parse response message from regmon.";

// "Real" implementation of RegmonClient talking to the Regmon daemon on the
// Chrome OS side.
class RegmonClientImpl : public RegmonClient {
 public:
  RegmonClientImpl() = default;
  RegmonClientImpl(const RegmonClientImpl&) = delete;
  RegmonClientImpl& operator=(const RegmonClientImpl&) = delete;
  ~RegmonClientImpl() override = default;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(regmon::kRegmonServiceName,
                                 dbus::ObjectPath(regmon::kRegmonServicePath));
  }

  void RecordPolicyViolation(
      const regmon::RecordPolicyViolationRequest request) override {
    dbus::MethodCall method_call(regmon::kRegmonServiceInterface,
                                 regmon::kRecordPolicyViolation);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode RecordPolicyViolationRequest protobuf";
      return;
    }

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&RegmonClientImpl::HandleRecordPolicyViolationResponse,
                       weak_factory_.GetWeakPtr(), std::move(request)));
  }

  // Tries to parse a proto message from |response| into |proto| and returns
  // null if successful. If |response| is nullptr or the message cannot be
  // parsed it will return an appropriate error message.
  const char* DeserializeProto(dbus::Response* response,
                               google::protobuf::MessageLite* proto) {
    if (!response) {
      return kNoResponseFailure;
    }

    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(proto)) {
      return kProtoMessageParsingFailure;
    }

    return nullptr;
  }

  void HandleRecordPolicyViolationResponse(
      const regmon::RecordPolicyViolationRequest request,
      dbus::Response* response) {
    regmon::RecordPolicyViolationResponse response_proto;
    const char* error_message = DeserializeProto(response, &response_proto);
    if (error_message) {
      LOG(ERROR) << "Regmon RecordPolicyViolation response failure: "
                 << error_message;
      return;
    }

    if (!response_proto.status().error_message().empty()) {
      LOG(ERROR) << "Regmon RecordPolicyViolation error: "
                 << response_proto.status().error_message();
    }
  }

 private:
  TestInterface* GetTestInterface() override { return nullptr; }

  // D-Bus proxy for the Regmon daemon, not owned.
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  base::WeakPtrFactory<RegmonClientImpl> weak_factory_{this};
};

}  // namespace

RegmonClient::RegmonClient() {
  CHECK(!g_instance);
  g_instance = this;
}

RegmonClient::~RegmonClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void RegmonClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new RegmonClientImpl())->Init(bus);
}

// static
void RegmonClient::InitializeFake() {
  new FakeRegmonClient();
}

// static
void RegmonClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
RegmonClient* RegmonClient::Get() {
  return g_instance;
}

}  // namespace chromeos
