// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/fake_missive_client.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"

namespace chromeos {

FakeMissiveClient::FakeMissiveClient() = default;

FakeMissiveClient::~FakeMissiveClient() = default;

void FakeMissiveClient::Init() {
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
  origin_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  is_initialized_ = true;
}

void FakeMissiveClient::EnqueueRecord(
    const reporting::Priority priority,
    reporting::Record record,
    base::OnceCallback<void(reporting::Status)> completion_callback) {
  for (auto& observer : observer_list_) {
    observer.OnRecordEnqueued(priority, record);
  }
  enqueued_records_[priority].push_back(std::move(record));
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

void FakeMissiveClient::UpdateConfigInMissive(
    const reporting::ListOfBlockedDestinations& destinations) {
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

const std::vector<::reporting::Record>& FakeMissiveClient::GetEnqueuedRecords(
    ::reporting::Priority priority) {
  return enqueued_records_[priority];
}

void FakeMissiveClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeMissiveClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace chromeos
