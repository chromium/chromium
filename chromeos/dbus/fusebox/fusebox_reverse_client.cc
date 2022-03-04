// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fusebox/fusebox_reverse_client.h"

#include "chromeos/dbus/fusebox/fake_fusebox_reverse_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

FuseBoxReverseClient* g_instance = nullptr;

class FuseBoxReverseClientImpl : public FuseBoxReverseClient {
 public:
  FuseBoxReverseClientImpl() = default;

  FuseBoxReverseClientImpl(const FuseBoxReverseClientImpl&) = delete;
  FuseBoxReverseClientImpl& operator=(const FuseBoxReverseClientImpl&) = delete;

  ~FuseBoxReverseClientImpl() override = default;

  void Init(dbus::Bus* bus);

  void ReplyToReadDir(uint64_t handle,
                      int32_t error_code,
                      fusebox::DirEntryListProto dir_entry_list_proto,
                      bool has_more) override;

 private:
  // D-BUS org.chromium.FuseBoxReverseClient proxy.
  dbus::ObjectProxy* proxy_ = nullptr;
};

void FuseBoxReverseClientImpl::Init(dbus::Bus* bus) {
  proxy_ = bus->GetObjectProxy(
      fusebox::kFuseBoxReverseServiceName,
      dbus::ObjectPath(fusebox::kFuseBoxReverseServicePath));
}

void FuseBoxReverseClientImpl::ReplyToReadDir(
    uint64_t handle,
    int32_t error_code,
    fusebox::DirEntryListProto dir_entry_list_proto,
    bool has_more) {
  dbus::MethodCall method(fusebox::kFuseBoxReverseServiceInterface,
                          fusebox::kReplyToReadDirMethod);

  dbus::MessageWriter writer(&method);
  writer.AppendUint64(handle);
  writer.AppendInt32(error_code);
  writer.AppendProtoAsArrayOfBytes(dir_entry_list_proto);
  writer.AppendBool(has_more);

  proxy_->CallMethod(&method, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::DoNothing());
}

}  // namespace

FuseBoxReverseClient::FuseBoxReverseClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FuseBoxReverseClient::~FuseBoxReverseClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void FuseBoxReverseClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new FuseBoxReverseClientImpl())->Init(bus);
}

// static
void FuseBoxReverseClient::InitializeFake() {
  if (!g_instance) {
    new FakeFuseBoxReverseClient();
  }
}

// static
void FuseBoxReverseClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
FuseBoxReverseClient* FuseBoxReverseClient::Get() {
  return g_instance;
}

}  // namespace chromeos
