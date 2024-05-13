// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MISSIVE_MISSIVE_STORAGE_MODULE_H_
#define CHROMEOS_DBUS_MISSIVE_MISSIVE_STORAGE_MODULE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace chromeos {

// MissiveStorageModule is a `StorageModuleInterface` implementation that
// channels enqueue and flush calls to `MissiveClient`.
class COMPONENT_EXPORT(MISSIVE) MissiveStorageModule
    : public ::reporting::StorageModuleInterface {
 public:
  // Factory method asynchronously creates `MissiveStorageModule` object.
  static void Create(
      base::OnceCallback<void(::reporting::StatusOr<scoped_refptr<
                                  ::reporting::StorageModuleInterface>>)> cb);

  MissiveStorageModule(const MissiveStorageModule& other) = delete;
  MissiveStorageModule& operator=(const MissiveStorageModule& other) = delete;

 private:
  friend base::RefCountedThreadSafe<MissiveStorageModule>;

  // Constructor can only be called by `Create` factory method.
  explicit MissiveStorageModule(MissiveClient* missive_client);

  // Refcounted object must have destructor declared protected or private.
  ~MissiveStorageModule() override;

  // Calls `MissiveClient::EnqueueRecord` forwarding the arguments.
  void AddRecord(::reporting::Priority priority,
                 ::reporting::Record record,
                 EnqueueCallback callback) override;

  // Calls `MissiveClient::Flush` to initiate upload of collected records
  // according to the priority. Called usually for a queue with an infinite or
  // very large upload period. Multiple `Flush` calls can safely run in
  // parallel. Returns error if cannot start upload.
  void Flush(::reporting::Priority priority, FlushCallback callback) override;

  const base::RepeatingCallback<void(::reporting::Priority,
                                     ::reporting::Record,
                                     MissiveStorageModule::EnqueueCallback)>
      add_record_action_;
  const base::RepeatingCallback<void(::reporting::Priority,
                                     MissiveStorageModule::FlushCallback)>
      flush_action_;
};
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MISSIVE_MISSIVE_STORAGE_MODULE_H_
