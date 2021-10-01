// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/missive_client.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/dbus/missive/fake_missive_client.h"
#include "components/reporting/proto/interface.pb.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

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
    DETACH_FROM_SEQUENCE(origin_checker_);

    DCHECK(!missive_service_proxy_);
    missive_service_proxy_ =
        bus->GetObjectProxy(missive::kMissiveServiceName,
                            dbus::ObjectPath(missive::kMissiveServicePath));
  }

  void EnqueueRecord(const reporting::Priority priority,
                     reporting::Record record,
                     base::OnceCallback<void(reporting::Status)>
                         completion_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    reporting::EnqueueRecordRequest request;
    *request.mutable_record() = std::move(record);
    request.set_priority(priority);

    dbus::MethodCall method_call(missive::kMissiveServiceInterface,
                                 missive::kEnqueueRecord);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    missive_service_proxy_->CallMethod(
        &method_call, kTimeoutMs,
        base::BindOnce(&MissiveClientImpl::HandleEnqueueRecordResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(completion_callback)));
  }

  void Flush(const reporting::Priority priority,
             base::OnceCallback<void(reporting::Status)> completion_callback)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    reporting::FlushPriorityRequest request;
    request.set_priority(priority);
    dbus::MethodCall method_call(missive::kMissiveServiceInterface,
                                 missive::kFlushPriority);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    missive_service_proxy_->CallMethod(
        &method_call, kTimeoutMs,
        base::BindOnce(&MissiveClientImpl::HandleFlushResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(completion_callback)));
  }

  void UpdateEncryptionKey(
      const reporting::SignedEncryptionInfo& encryption_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
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

  void ReportSuccess(
      const reporting::SequencingInformation& sequencing_information,
      bool force_confirm) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
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

  MissiveClient::TestInterface* GetTestInterface() override { return nullptr; }

  base::WeakPtr<MissiveClient> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void HandleEnqueueRecordResponse(
      base::OnceCallback<void(reporting::Status)> completion_callback,
      dbus::Response* response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
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

  void HandleFlushResponse(
      base::OnceCallback<void(reporting::Status)> completion_callback,
      dbus::Response* response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
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

  scoped_refptr<dbus::ObjectProxy> missive_service_proxy_;

  // Weak pointer factory - must be last member of the class.
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

scoped_refptr<base::SequencedTaskRunner> MissiveClient::origin_task_runner()
    const {
  return origin_task_runner_;
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

}  // namespace chromeos
