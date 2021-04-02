// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/missive_storage_module_delegate_impl.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"

namespace reporting {

MissiveStorageModuleDelegateImpl::MissiveStorageModuleDelegateImpl(
    AddRecordCallback add_record,
    FlushCallback flush,
    ReportSuccessCallback report_success,
    UpdateEncryptionKeyCallback update_encryption_key)
    : add_record_(std::move(add_record)),
      flush_(std::move(flush)),
      report_success_(std::move(report_success)),
      update_encryption_key_(std::move(update_encryption_key)) {}

MissiveStorageModuleDelegateImpl::~MissiveStorageModuleDelegateImpl() = default;

void MissiveStorageModuleDelegateImpl::AddRecord(
    const Priority priority,
    Record record,
    base::OnceCallback<void(Status)> callback) {
  add_record_.Run(priority, std::move(record), std::move(callback));
}

void MissiveStorageModuleDelegateImpl::Flush(
    const Priority priority,
    base::OnceCallback<void(Status)> callback) {
  flush_.Run(priority, std::move(callback));
}

void MissiveStorageModuleDelegateImpl::ReportSuccess(
    const SequencingInformation& sequencing_information,
    bool force) {
  report_success_.Run(sequencing_information, force);
}

void MissiveStorageModuleDelegateImpl::UpdateEncryptionKey(
    const SignedEncryptionInfo& signed_encryption_key) {
  update_encryption_key_.Run(signed_encryption_key);
}

}  // namespace reporting
