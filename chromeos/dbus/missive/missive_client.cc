// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/missive_client.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/missive/fake_missive_client.h"
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

class MissiveClientImpl : public MissiveClient {
 public:
  MissiveClientImpl() = default;
  ~MissiveClientImpl() override = default;

  MissiveClientImpl(const MissiveClientImpl& other) = delete;
  MissiveClientImpl& operator=(const MissiveClientImpl& other) = delete;
  void Init(dbus::Bus* const bus) {
    DCHECK(!missive_service_proxy_);
    auto missive_storage_module_delegate =
        std::make_unique<MissiveStorageModuleDelegateImpl>(
            base::BindRepeating(&MissiveClientImpl::AddRecord,
                                weak_ptr_factory_.GetWeakPtr()),
            base::BindRepeating(&MissiveClientImpl::Flush,
                                weak_ptr_factory_.GetWeakPtr()),
            base::BindRepeating(&MissiveClientImpl::ReportSuccess,
                                weak_ptr_factory_.GetWeakPtr()),
            base::BindRepeating(&MissiveClientImpl::UpdateEncryptionKey,
                                weak_ptr_factory_.GetWeakPtr()));
    missive_storage_module_ = MissiveStorageModule::Create(
        std::move(missive_storage_module_delegate));

    missive_service_proxy_ =
        bus->GetObjectProxy(missive::kMissiveServiceName,
                            dbus::ObjectPath(missive::kMissiveServicePath));
  }

 private:
  void AddRecord(const reporting::Priority priority,
                 const reporting::Record& record,
                 base::OnceCallback<void(reporting::Status)>
                     completion_callback) override {
    // TODO(1174889): Implement the actual DBus Call after the Daemon is
    // available.
    std::move(completion_callback)
        .Run(reporting::Status(reporting::error::UNAVAILABLE,
                               "AddRecord has not been implemented."));
  }

  void Flush(const reporting::Priority priority,
             base::OnceCallback<void(reporting::Status)> completion_callback)
      override {
    // TODO(1174889): Implement the actual DBus Call after the Daemon is
    // available.
    std::move(reporting::Status(reporting::error::UNAVAILABLE,
                                "Flush has not been implemented."));
  }

  void ReportSuccess(
      const reporting::SequencingInformation& sequencing_information,
      bool force_confirm) override {
    // TODO(1174889): Implement the actual DBus Call after the Daemon is
    // available.
    return;
  }

  void UpdateEncryptionKey(
      const reporting::SignedEncryptionInfo& encryption_info) override {
    // TODO(1174889): Implement the actual DBus Call after the Daemon is
    // available.
    return;
  }

  dbus::ObjectProxy* missive_service_proxy_ = nullptr;

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
