// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_MISSIVE_STORAGE_MODULE_H_
#define COMPONENTS_REPORTING_STORAGE_MISSIVE_STORAGE_MODULE_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"

namespace reporting {

// MissiveStorageModule is a StorageModuleInterface implementation forwarded to
// MissiveClient (it utilizes a Delegate and channels all calls through it).
class MissiveStorageModule : public StorageModuleInterface {
 public:
  // MissiveStorageModuleDelegateInterface has the same interface as
  // StorageModuleInterface but isn't shared or created as a scoped_refptr.
  // MissiveStorageModuleDelegateInterface is expected to be implemented by the
  // caller.
  class MissiveStorageModuleDelegateInterface {
   public:
    MissiveStorageModuleDelegateInterface();
    virtual ~MissiveStorageModuleDelegateInterface();
    MissiveStorageModuleDelegateInterface(
        const MissiveStorageModuleDelegateInterface& other) = delete;
    MissiveStorageModuleDelegateInterface& operator=(
        const MissiveStorageModuleDelegateInterface& other) = delete;

    virtual void AddRecord(const Priority priority,
                           Record record,
                           EnqueueCallback callback) = 0;
    virtual void Flush(Priority priority, FlushCallback callback) = 0;
  };

  // Factory method creates |MissiveStorageModule| object.
  static scoped_refptr<MissiveStorageModule> Create(
      std::unique_ptr<MissiveStorageModuleDelegateInterface> delegate);

  MissiveStorageModule(const MissiveStorageModule& other) = delete;
  MissiveStorageModule& operator=(const MissiveStorageModule& other) = delete;

  // Calls |missive_delegate_->AddRecord| forwarding the arguments.
  void AddRecord(Priority priority,
                 Record record,
                 EnqueueCallback callback) override;

  // Calls |missive_delegate_->Flush| to initiate upload of collected records
  // according to the priority. Called usually for a queue with an infinite or
  // very large upload period. Multiple |Flush| calls can safely run in
  // parallel. Returns error if cannot start upload.
  void Flush(Priority priority, FlushCallback callback) override;

 protected:
  // Constructor can only be called by |Create| factory method.
  explicit MissiveStorageModule(
      std::unique_ptr<MissiveStorageModuleDelegateInterface> delegate);

  // Refcounted object must have destructor declared protected or private.
  ~MissiveStorageModule() override;

 private:
  friend base::RefCountedThreadSafe<MissiveStorageModule>;

  std::unique_ptr<MissiveStorageModuleDelegateInterface> delegate_;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_MISSIVE_STORAGE_MODULE_H_
