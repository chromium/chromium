// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_MISSIVE_STORAGE_MODULE_DELEGATE_IMPL_H_
#define COMPONENTS_REPORTING_STORAGE_MISSIVE_STORAGE_MODULE_DELEGATE_IMPL_H_

#include "base/callback.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/missive_storage_module.h"
#include "components/reporting/util/status.h"

namespace reporting {

// Provides a delegate that sends all requests to callbacks.
class MissiveStorageModuleDelegateImpl
    : public MissiveStorageModule::MissiveStorageModuleDelegateInterface {
 public:
  using AddRecordCallback = base::RepeatingCallback<
      void(Priority, Record, base::OnceCallback<void(Status)>)>;
  using FlushCallback =
      base::RepeatingCallback<void(Priority, base::OnceCallback<void(Status)>)>;

  MissiveStorageModuleDelegateImpl(AddRecordCallback add_record,
                                   FlushCallback flush);
  ~MissiveStorageModuleDelegateImpl() override;

  void AddRecord(Priority priority,
                 Record record,
                 base::OnceCallback<void(Status)> callback) override;

  void Flush(Priority priority,
             base::OnceCallback<void(Status)> callback) override;

  void ReportSuccess(const SequenceInformation& sequence_information,
                     bool force) override;

  void UpdateEncryptionKey(
      const SignedEncryptionInfo& signed_encryption_key) override;

 private:
  const AddRecordCallback add_record_;
  const FlushCallback flush_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_MISSIVE_STORAGE_MODULE_DELEGATE_IMPL_H_
