// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/missive_client.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/missive/fake_missive_client.h"
#include "components/reporting/proto/interface.pb.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/storage/missive_storage_module.h"
#include "components/reporting/storage/missive_storage_module_delegate_impl.h"
#include "components/reporting/util/status.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

using reporting::MissiveStorageModule;
using reporting::MissiveStorageModuleDelegateImpl;
using reporting::Priority;
using reporting::Record;
using reporting::SequencingInformation;
using reporting::SignedEncryptionInfo;
using reporting::Status;

namespace {

MissiveClient* g_instance = nullptr;

// Amount of time we wait for a response before timing out. The default value is
// 25 seconds.
const int kTimeoutMs = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT;

class MissiveClientImpl : public MissiveClient {
 public:
  MissiveClientImpl() = default;
  MissiveClientImpl(const MissiveClientImpl& other) = delete;
  MissiveClientImpl& operator=(const MissiveClientImpl& other) = delete;
  ~MissiveClientImpl() override = default;

  void Init(dbus::Bus* const bus) {
    origin_task_runner_ = bus->GetOriginTaskRunner();

    DCHECK(!missive_service_proxy_);

    auto missive_storage_module_delegate =
        std::make_unique<MissiveStorageModuleDelegateImpl>(
            base::BindPostTask(
                origin_task_runner_,
                base::BindRepeating(&MissiveClientImpl::EnqueueRecord,
                                    weak_ptr_factory_.GetWeakPtr())),
            base::BindPostTask(
                origin_task_runner_,
                base::BindRepeating(&MissiveClientImpl::Flush,
                                    weak_ptr_factory_.GetWeakPtr())),
            base::BindPostTask(
                origin_task_runner_,
                base::BindRepeating(&MissiveClientImpl::ReportSuccess,
                                    weak_ptr_factory_.GetWeakPtr())),
            base::BindPostTask(
                origin_task_runner_,
                base::BindRepeating(&MissiveClientImpl::UpdateEncryptionKey,
                                    weak_ptr_factory_.GetWeakPtr())));
    missive_storage_module_ = MissiveStorageModule::Create(
        std::move(missive_storage_module_delegate));

    missive_service_proxy_ =
        bus->GetObjectProxy(missive::kMissiveServiceName,
                            dbus::ObjectPath(missive::kMissiveServicePath));
  }

 private:
  static void EnqueueRecord(
      base::WeakPtr<MissiveClientImpl> client,
      const reporting::Priority priority,
      reporting::Record record,
      base::OnceCallback<void(reporting::Status)> completion_callback) {
    if (!client) {
      std::move(completion_callback)
          .Run(Status(reporting::error::UNAVAILABLE,
                      "Missive client shut down"));
      return;
    }

    reporting::EnqueueRecordRequest request;
    *request.mutable_record() = std::move(record);
    request.set_priority(priority);

    dbus::MethodCall method_call(missive::kMissiveServiceInterface,
                                 missive::kEnqueueRecord);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    client->missive_service_proxy_->CallMethod(
        &method_call, kTimeoutMs,
        base::BindOnce(&MissiveClientImpl::HandleEnqueueRecordResponse, client,
                       std::move(completion_callback)));
  }

  static void HandleEnqueueRecordResponse(
      base::WeakPtr<MissiveClientImpl> client,
      base::OnceCallback<void(reporting::Status)> completion_callback,
      dbus::Response* response) {
    if (!client) {
      std::move(completion_callback)
          .Run(Status(reporting::error::UNAVAILABLE,
                      "Missive client shut down"));
      return;
    }

    if (!response) {
      std::move(completion_callback)
          .Run(Status(reporting::error::UNAVAILABLE,
                      "EnqueueRecord is not exported by missived"));
      return;
    }
    dbus::MessageReader reader(response);
    reporting::EnqueueRecordResponse response_body;
    reader.PopArrayOfBytesAsProto(&response_body);

    reporting::Status status;
    status.RestoreFrom(response_body.status());
    std::move(completion_callback).Run(status);
  }

  static void Flush(
      base::WeakPtr<MissiveClientImpl> client,
      const reporting::Priority priority,
      base::OnceCallback<void(reporting::Status)> completion_callback) {
    if (!client) {
      std::move(completion_callback)
          .Run(Status(reporting::error::UNAVAILABLE,
                      "Missive client shut down"));
      return;
    }

    reporting::FlushPriorityRequest request;
    request.set_priority(priority);
    dbus::MethodCall method_call(missive::kMissiveServiceInterface,
                                 missive::kFlushPriority);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    client->missive_service_proxy_->CallMethod(
        &method_call, kTimeoutMs,
        base::BindOnce(&MissiveClientImpl::HandleFlushResponse, client,
                       std::move(completion_callback)));
  }

  static void HandleFlushResponse(
      base::WeakPtr<MissiveClientImpl> client,
      base::OnceCallback<void(reporting::Status)> completion_callback,
      dbus::Response* response) {
    if (!client) {
      std::move(completion_callback)
          .Run(Status(reporting::error::UNAVAILABLE,
                      "Missive client shut down"));
      return;
    }

    if (!response) {
      std::move(completion_callback)
          .Run(Status(reporting::error::UNAVAILABLE,
                      "HandleFlushResponse is not exported by missived"));
      return;
    }
    dbus::MessageReader reader(response);
    reporting::FlushPriorityResponse response_body;
    reader.PopArrayOfBytesAsProto(&response_body);

    reporting::Status status;
    status.RestoreFrom(response_body.status());
    std::move(completion_callback).Run(status);
  }

  void ReportSuccess(
      const reporting::SequencingInformation& sequencing_information,
      bool force_confirm) {
    reporting::ConfirmRecordUploadRequest request;
    *request.mutable_sequencing_information() =
        std::move(sequencing_information);
    request.set_force_confirm(force_confirm);
    dbus::MethodCall method_call(missive::kMissiveServiceInterface,
                                 missive::kConfirmRecordUpload);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    missive_service_proxy_->CallMethod(&method_call, kTimeoutMs,
                                       base::DoNothing());
  }

  void UpdateEncryptionKey(
      const reporting::SignedEncryptionInfo& encryption_info) {
    reporting::UpdateEncryptionKeyRequest request;
    *request.mutable_signed_encryption_info() = std::move(encryption_info);
    dbus::MethodCall method_call(missive::kMissiveServiceInterface,
                                 missive::kUpdateEncryptionKey);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    missive_service_proxy_->CallMethod(&method_call, kTimeoutMs,
                                       base::DoNothing());
    return;
  }

  scoped_refptr<dbus::ObjectProxy> missive_service_proxy_;

  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;

  // Must be last class member.
  base::WeakPtrFactory<MissiveClientImpl> weak_ptr_factory_{this};
};

}  // namespace

MissiveClient::MissiveClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

MissiveClient::~MissiveClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void MissiveClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new MissiveClientImpl())->Init(bus);
}

// static
void MissiveClient::InitializeFake() {
  (new FakeMissiveClient())->Init();
}

// static
void MissiveClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
MissiveClient* MissiveClient::Get() {
  return g_instance;
}

scoped_refptr<MissiveStorageModule> MissiveClient::GetMissiveStorageModule() {
  DCHECK(g_instance);
  return g_instance->missive_storage_module_;
}

}  // namespace chromeos
