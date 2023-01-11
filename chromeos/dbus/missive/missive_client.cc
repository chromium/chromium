// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/missive_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/dbus/missive/fake_missive_client.h"
#include "components/reporting/proto/synced/interface.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/disconnectable_client.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

using reporting::Priority;
using reporting::Record;
using reporting::SequenceInformation;
using reporting::SignedEncryptionInfo;
using reporting::Status;

namespace {

MissiveClient* g_instance = nullptr;

class MissiveClientImpl : public MissiveClient {
 public:
  MissiveClientImpl() : client_(origin_task_runner()) {}
  MissiveClientImpl(const MissiveClientImpl& other) = delete;
  MissiveClientImpl& operator=(const MissiveClientImpl& other) = delete;
  ~MissiveClientImpl() override = default;

  void Init(dbus::Bus* const bus) {
    DCHECK(bus);
    origin_task_runner_ = bus->GetOriginTaskRunner();

    DCHECK(!missive_service_proxy_);
    missive_service_proxy_ =
        bus->GetObjectProxy(missive::kMissiveServiceName,
                            dbus::ObjectPath(missive::kMissiveServicePath));
    missive_service_proxy_->SetNameOwnerChangedCallback(base::BindRepeating(
        &MissiveClientImpl::OwnerChanged, weak_ptr_factory_.GetWeakPtr()));
    missive_service_proxy_->WaitForServiceToBeAvailable(base::BindOnce(
        &MissiveClientImpl::ServerAvailable, weak_ptr_factory_.GetWeakPtr()));
  }

  void EnqueueRecord(const reporting::Priority priority,
                     reporting::Record record,
                     base::OnceCallback<void(reporting::Status)>
                         completion_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    auto delegate = std::make_unique<EnqueueRecordDelegate>(
        priority, std::move(record), this, std::move(completion_callback));
    client_.MaybeMakeCall(std::move(delegate));
  }

  void Flush(const reporting::Priority priority,
             base::OnceCallback<void(reporting::Status)> completion_callback)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    auto delegate = std::make_unique<FlushDelegate>(
        priority, this, std::move(completion_callback));
    client_.MaybeMakeCall(std::move(delegate));
  }

  void UpdateEncryptionKey(
      const reporting::SignedEncryptionInfo& encryption_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    auto delegate =
        std::make_unique<UpdateEncryptionKeyDelegate>(encryption_info, this);
    client_.MaybeMakeCall(std::move(delegate));
  }

  void ReportSuccess(const reporting::SequenceInformation& sequence_information,
                     bool force_confirm) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    auto delegate = std::make_unique<ReportSuccessDelegate>(
        sequence_information, force_confirm, this);
    client_.MaybeMakeCall(std::move(delegate));
  }

  MissiveClient::TestInterface* GetTestInterface() override { return nullptr; }

  base::WeakPtr<MissiveClient> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Class implements DisconnectableClient::Delegate specifically for dBus
  // calls. Logic that handles dBus connect/disconnect cases remains with the
  // base class.
  class DBusDelegate : public reporting::DisconnectableClient::Delegate {
   public:
    DBusDelegate(const DBusDelegate& other) = delete;
    DBusDelegate& operator=(const DBusDelegate& other) = delete;
    ~DBusDelegate() override = default;

    // Writes request into dBus message writer.
    virtual bool WriteRequest(dbus::MessageWriter* writer) = 0;

    // Parses response, retrieves status information from it and returns it.
    // Optional - returns OK if absent.
    virtual reporting::Status ParseResponse(dbus::MessageReader* reader) {
      return reporting::Status::StatusOK();
    }

   protected:
    DBusDelegate(
        const char* dbus_method,
        MissiveClientImpl* owner,
        base::OnceCallback<void(reporting::Status)> completion_callback)
        : dbus_method_(dbus_method),
          owner_(owner),
          completion_callback_(std::move(completion_callback)) {}

   private:
    // Implementation of DisconnectableClient::Delegate.
    void DoCall(base::OnceClosure cb) final {
      DCHECK_CALLED_ON_VALID_SEQUENCE(owner_->origin_checker_);
      base::ScopedClosureRunner autorun(std::move(cb));
      dbus::MethodCall method_call(missive::kMissiveServiceInterface,
                                   dbus_method_);
      dbus::MessageWriter writer(&method_call);
      if (!WriteRequest(&writer)) {
        reporting::Status status(
            reporting::error::UNKNOWN,
            "MessageWriter was unable to append the request.");
        LOG(ERROR) << status;
        std::move(completion_callback_).Run(status);
        return;
      }

      // Make a dBus call.
      owner_->missive_service_proxy_->CallMethod(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
          base::BindOnce(
              [](base::ScopedClosureRunner autorun,
                 base::WeakPtr<DBusDelegate> self, dbus::Response* response) {
                if (!self) {
                  return;  // Delegate already deleted.
                }
                DCHECK_CALLED_ON_VALID_SEQUENCE(self->owner_->origin_checker_);
                if (!response) {
                  self->Respond(Status(reporting::error::UNAVAILABLE,
                                       "Returned no response"));
                  return;
                }
                self->response_ = response;
              },
              std::move(autorun), weak_ptr_factory_.GetWeakPtr()));
    }

    // Process dBus response, if status is OK, or error otherwise.
    void Respond(reporting::Status status) final {
      DCHECK_CALLED_ON_VALID_SEQUENCE(owner_->origin_checker_);
      if (!completion_callback_) {
        return;
      }
      if (status.ok()) {
        dbus::MessageReader reader(response_);
        status = ParseResponse(&reader);
      }
      std::move(completion_callback_).Run(status);
    }

    const char* const dbus_method_;
    raw_ptr<dbus::Response> response_;
    const raw_ptr<MissiveClientImpl> owner_;
    base::OnceCallback<void(reporting::Status)> completion_callback_;

    // Weak pointer factory - must be last member of the class.
    base::WeakPtrFactory<DBusDelegate> weak_ptr_factory_{this};
  };

  class EnqueueRecordDelegate : public DBusDelegate {
   public:
    EnqueueRecordDelegate(
        reporting::Priority priority,
        reporting::Record record,
        MissiveClientImpl* owner,
        base::OnceCallback<void(reporting::Status)> completion_callback)
        : DBusDelegate(missive::kEnqueueRecord,
                       owner,
                       std::move(completion_callback)) {
      *request_.mutable_record() = std::move(record);
      request_.set_priority(priority);
    }

    bool WriteRequest(dbus::MessageWriter* writer) override {
      return writer->AppendProtoAsArrayOfBytes(request_);
    }

    reporting::Status ParseResponse(dbus::MessageReader* reader) override {
      reporting::EnqueueRecordResponse response_body;
      if (!reader->PopArrayOfBytesAsProto(&response_body)) {
        return reporting::Status(reporting::error::INTERNAL,
                                 "Response was not parsable.");
      }
      reporting::Status status;
      status.RestoreFrom(response_body.status());
      return status;
    }

   private:
    reporting::EnqueueRecordRequest request_;
  };

  class FlushDelegate : public DBusDelegate {
   public:
    FlushDelegate(
        reporting::Priority priority,
        MissiveClientImpl* owner,
        base::OnceCallback<void(reporting::Status)> completion_callback)
        : DBusDelegate(missive::kFlushPriority,
                       owner,
                       std::move(completion_callback)) {
      request_.set_priority(priority);
    }

    bool WriteRequest(dbus::MessageWriter* writer) override {
      return writer->AppendProtoAsArrayOfBytes(request_);
    }

    reporting::Status ParseResponse(dbus::MessageReader* reader) override {
      reporting::FlushPriorityResponse response_body;
      if (!reader->PopArrayOfBytesAsProto(&response_body)) {
        return reporting::Status(reporting::error::INTERNAL,
                                 "Response was not parsable.");
      }
      reporting::Status status;
      status.RestoreFrom(response_body.status());
      return status;
    }

   private:
    reporting::FlushPriorityRequest request_;
  };

  class UpdateEncryptionKeyDelegate : public DBusDelegate {
   public:
    UpdateEncryptionKeyDelegate(
        const reporting::SignedEncryptionInfo& encryption_info,
        MissiveClientImpl* owner)
        : DBusDelegate(missive::kUpdateEncryptionKey,
                       owner,
                       base::DoNothing()) {
      *request_.mutable_signed_encryption_info() = encryption_info;
    }

    bool WriteRequest(dbus::MessageWriter* writer) override {
      return writer->AppendProtoAsArrayOfBytes(request_);
    }

   private:
    reporting::UpdateEncryptionKeyRequest request_;
  };

  class ReportSuccessDelegate : public DBusDelegate {
   public:
    ReportSuccessDelegate(
        const reporting::SequenceInformation& sequence_information,
        bool force_confirm,
        MissiveClientImpl* owner)
        : DBusDelegate(missive::kConfirmRecordUpload,
                       owner,
                       base::DoNothing()) {
      *request_.mutable_sequence_information() = sequence_information;
      request_.set_force_confirm(force_confirm);
    }

    bool WriteRequest(dbus::MessageWriter* writer) override {
      return writer->AppendProtoAsArrayOfBytes(request_);
    }

   private:
    reporting::ConfirmRecordUploadRequest request_;
  };

  void OwnerChanged(const std::string& old_owner,
                    const std::string& new_owner) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    client_.SetAvailability(/*is_available=*/!new_owner.empty());
  }

  void ServerAvailable(bool service_is_available) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    client_.SetAvailability(/*is_available=*/service_is_available);
  }

  scoped_refptr<dbus::ObjectProxy> missive_service_proxy_;

  reporting::DisconnectableClient client_;

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
