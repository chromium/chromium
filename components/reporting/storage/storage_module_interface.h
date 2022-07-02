// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_STORAGE_MODULE_INTERFACE_H_
#define COMPONENTS_REPORTING_STORAGE_STORAGE_MODULE_INTERFACE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"

namespace reporting {

class StorageModuleInterface
    : public base::RefCountedThreadSafe<StorageModuleInterface> {
 public:
  using EnqueueCallback = base::OnceCallback<void(Status)>;
  using FlushCallback = base::OnceCallback<void(Status)>;

  StorageModuleInterface(const StorageModuleInterface& other) = delete;
  StorageModuleInterface& operator=(const StorageModuleInterface& other) =
      delete;

  // AddRecord will add |record| (taking ownership) to the
  // |StorageModuleInterface| according to the provided |priority|. On
  // completion, |callback| is called.
  virtual void AddRecord(Priority priority,
                         Record record,
                         EnqueueCallback callback) = 0;

  // Initiates upload of collected records according to the priority.
  // Called usually for a queue with an infinite or very large upload period.
  // Multiple |Flush| calls can safely run in parallel.
  // Returns error if cannot start upload.
  virtual void Flush(Priority priority, FlushCallback callback) = 0;

  // Once a record has been successfully uploaded, the sequence information
  // can be passed back to the StorageModuleInterface here for record deletion.
  // If |force| is false (which is used in most cases), |sequence_information|
  // only affects Storage if no higher sequencing was confirmed before;
  // otherwise it is accepted unconditionally.
  virtual void ReportSuccess(SequenceInformation sequence_information,
                             bool force) = 0;

  // If the server attached signed encryption key to the response, it needs to
  // be paased here.
  virtual void UpdateEncryptionKey(
      SignedEncryptionInfo signed_encryption_key) = 0;

 protected:
  // Constructor can only be called by |Create| factory method.
  StorageModuleInterface();

  // Refcounted object must have destructor declared protected or private.
  virtual ~StorageModuleInterface();

 private:
  friend base::RefCountedThreadSafe<StorageModuleInterface>;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_STORAGE_MODULE_INTERFACE_H_
