// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/missive_storage_module.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"

namespace chromeos {

MissiveStorageModule::MissiveStorageModule(MissiveClient* missive_client)
    : add_record_action_(base::BindPostTask(
          missive_client->origin_task_runner(),
          base::BindRepeating(&MissiveClient::EnqueueRecord,
                              missive_client->GetWeakPtr()))),
      flush_action_(base::BindPostTask(
          missive_client->origin_task_runner(),
          base::BindRepeating(&MissiveClient::Flush,
                              missive_client->GetWeakPtr()))) {}

MissiveStorageModule::~MissiveStorageModule() = default;

// static
void MissiveStorageModule::Create(
    base::OnceCallback<void(::reporting::StatusOr<scoped_refptr<
                                ::reporting::StorageModuleInterface>>)> cb) {
  MissiveClient* const missive_client = MissiveClient::Get();
  if (!missive_client) {
    std::move(cb).Run(base::unexpected(::reporting::Status(
        ::reporting::error::FAILED_PRECONDITION,
        "Missive Client unavailable, probably has not been initialized")));
    return;
  }
  // Refer to the storage module.
  auto missive_storage_module =
      base::WrapRefCounted(new MissiveStorageModule(missive_client));
  LOG(WARNING) << "Store reporting data by a Missive daemon";
  std::move(cb).Run(missive_storage_module);
  return;
}

void MissiveStorageModule::AddRecord(::reporting::Priority priority,
                                     ::reporting::Record record,
                                     EnqueueCallback callback) {
  add_record_action_.Run(priority, std::move(record), std::move(callback));
}

void MissiveStorageModule::Flush(::reporting::Priority priority,
                                 FlushCallback callback) {
  flush_action_.Run(priority, std::move(callback));
}
}  // namespace chromeos
