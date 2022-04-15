// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fusebox/fusebox_reverse_client.h"

#include <errno.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/dbus/fusebox/fake_fusebox_reverse_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

FuseBoxReverseClient* g_instance = nullptr;

class FuseBoxReverseClientImpl : public FuseBoxReverseClient {
 public:
  FuseBoxReverseClientImpl() = default;

  FuseBoxReverseClientImpl(const FuseBoxReverseClientImpl&) = delete;
  FuseBoxReverseClientImpl& operator=(const FuseBoxReverseClientImpl&) = delete;

  ~FuseBoxReverseClientImpl() override = default;

  void Init(dbus::Bus* bus);

  void AttachStorage(const std::string& name, StorageResult callback) override;

  void DetachStorage(const std::string& name, StorageResult callback) override;

  void ReplyToReadDir(uint64_t handle,
                      int32_t error_code,
                      fusebox::DirEntryListProto dir_entry_list_proto,
                      bool has_more) override;

 private:
  // Calls fusebox storage |service| method with |name|.
  void StorageRequest(const char* service,
                      const std::string& name,
                      StorageResult callback);

  // Handles fusebox storage |service| method response.
  void StorageResponse(const char* service,
                       const std::string& name,
                       StorageResult callback,
                       dbus::Response* response);

  // Returns base::WeakPtr{this}.
  base::WeakPtr<FuseBoxReverseClientImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // D-BUS org.chromium.FuseBoxReverseClient proxy.
  dbus::ObjectProxy* proxy_ = nullptr;

  // base::WeakPtr{this} factory.
  base::WeakPtrFactory<FuseBoxReverseClientImpl> weak_ptr_factory_{this};
};

void FuseBoxReverseClientImpl::Init(dbus::Bus* bus) {
  const auto path = dbus::ObjectPath(fusebox::kFuseBoxReverseServicePath);
  proxy_ = bus->GetObjectProxy(fusebox::kFuseBoxReverseServiceName, path);
  DCHECK(proxy_);
}

void FuseBoxReverseClientImpl::AttachStorage(const std::string& name,
                                             StorageResult callback) {
  if (!g_instance || !proxy_) {
    std::move(callback).Run(ENODEV);
    return;
  }

  StorageRequest(fusebox::kAttachStorageMethod, name, std::move(callback));
}

void FuseBoxReverseClientImpl::DetachStorage(const std::string& name,
                                             StorageResult callback) {
  if (!g_instance || !proxy_) {
    std::move(callback).Run(ENODEV);
    return;
  }

  StorageRequest(fusebox::kDetachStorageMethod, name, std::move(callback));
}

void FuseBoxReverseClientImpl::StorageRequest(const char* service,
                                              const std::string& name,
                                              StorageResult callback) {
  dbus::MethodCall method(fusebox::kFuseBoxReverseServiceInterface, service);

  dbus::MessageWriter writer(&method);
  writer.AppendString(name);

  auto storage_response =
      base::BindOnce(&FuseBoxReverseClientImpl::StorageResponse, GetWeakPtr(),
                     service, name, std::move(callback));

  proxy_->CallMethod(&method, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     std::move(storage_response));
}

void FuseBoxReverseClientImpl::StorageResponse(const char* service,
                                               const std::string& name,
                                               StorageResult callback,
                                               dbus::Response* response) {
  int error = ETIMEDOUT;
  dbus::MessageReader reader(response);
  if (response && !reader.PopInt32(&error))
    error = EINVAL;

  LOG_IF(ERROR, error) << base::JoinString(
      {service, name, base::safe_strerror(error)}, " ");
  std::move(callback).Run(error);
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

}  // namespace ash
