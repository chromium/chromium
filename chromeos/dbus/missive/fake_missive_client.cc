// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/fake_missive_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"

namespace chromeos {

FakeMissiveClient::FakeMissiveClient() = default;

FakeMissiveClient::~FakeMissiveClient() = default;

void FakeMissiveClient::Init() {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  origin_task_runner_ = base::SequencedTaskRunnerHandle::Get();
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
    const reporting::SequenceInformation& sequence_information,
    bool force_confirm) {
  return;
}

void FakeMissiveClient::UpdateEncryptionKey(
    const reporting::SignedEncryptionInfo& encryption_info) {
  return;
}

MissiveClient::TestInterface* FakeMissiveClient::GetTestInterface() {
  return this;
}

base::WeakPtr<MissiveClient> FakeMissiveClient::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace chromeos
