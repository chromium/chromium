// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/fake_missive_client.h"

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/storage/missive_storage_module.h"
#include "components/reporting/storage/missive_storage_module_delegate_impl.h"
#include "components/reporting/util/status.h"

namespace chromeos {

using reporting::MissiveStorageModule;
using reporting::MissiveStorageModuleDelegateImpl;

FakeMissiveClient::FakeMissiveClient() = default;

FakeMissiveClient::~FakeMissiveClient() = default;

void FakeMissiveClient::Init() {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  sequenced_task_runner_ = base::SequencedTaskRunnerHandle::Get();

  auto missive_storage_module_delegate =
      std::make_unique<MissiveStorageModuleDelegateImpl>(
          base::BindPostTask(
              sequenced_task_runner_,
              base::BindRepeating(&FakeMissiveClient::EnqueueRecord,
                                  weak_ptr_factory_.GetWeakPtr())),
          base::BindPostTask(
              sequenced_task_runner_,
              base::BindRepeating(&FakeMissiveClient::Flush,
                                  weak_ptr_factory_.GetWeakPtr())),
          base::BindPostTask(
              sequenced_task_runner_,
              base::BindRepeating(&FakeMissiveClient::ReportSuccess,
                                  weak_ptr_factory_.GetWeakPtr())),
          base::BindPostTask(
              sequenced_task_runner_,
              base::BindRepeating(&FakeMissiveClient::UpdateEncryptionKey,
                                  weak_ptr_factory_.GetWeakPtr())));
  missive_storage_module_ =
      MissiveStorageModule::Create(std::move(missive_storage_module_delegate));
}

void FakeMissiveClient::EnqueueRecord(
    const reporting::Priority priority,
    reporting::Record record,
    base::OnceCallback<void(reporting::Status)> completion_callback) {
  std::move(completion_callback).Run(reporting::Status::StatusOK());
}

void FakeMissiveClient::Flush(
    const reporting::Priority priority,
    base::OnceCallback<void(reporting::Status)> completion_callback) {
  std::move(completion_callback).Run(reporting::Status::StatusOK());
}

void FakeMissiveClient::ReportSuccess(
    const reporting::SequencingInformation& sequencing_information,
    bool force_confirm) {
  return;
}

void FakeMissiveClient::UpdateEncryptionKey(
    const reporting::SignedEncryptionInfo& encryption_info) {
  return;
}

}  // namespace chromeos
