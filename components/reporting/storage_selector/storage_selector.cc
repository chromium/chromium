// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage_selector/storage_selector.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/task/bind_post_task.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/storage/missive_storage_module.h"
#include "components/reporting/storage/missive_storage_module_delegate_impl.h"

using ::chromeos::MissiveClient;

#endif  // BUILDFLAG(IS_CHROMEOS)

namespace reporting {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
// Features settings for storage and uploader.
// Use `missived` by all browsers.
BASE_FEATURE(kUseMissiveDaemonFeature,
             "ConnectMissiveDaemon",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Receive `missived` uploads by ASH/primary browser only.
BASE_FEATURE(kProvideUploaderFeature,
             "ProvideUploader",
#if BUILDFLAG(IS_CHROMEOS_ASH)
             base::FEATURE_ENABLED_BY_DEFAULT
#else   // BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

// static
bool StorageSelector::is_uploader_required() {
#if BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(kProvideUploaderFeature);
#else   // Not ChromeOS
  return true;   // Local storage must have an uploader.
#endif  // BUILDFLAG(IS_CHROMEOS)
}

// static
bool StorageSelector::is_use_missive() {
#if BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(kUseMissiveDaemonFeature);
#else   // Not ChromeOS
  return false;  // Use Local storage.
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
// static
void StorageSelector::CreateMissiveStorageModule(
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>
        cb) {
  MissiveClient* const missive_client = MissiveClient::Get();
  if (!missive_client) {
    std::move(cb).Run(Status(
        error::FAILED_PRECONDITION,
        "Missive Client unavailable, probably has not been initialized"));
    return;
  }
  // Refer to the storage module.
  auto missive_storage_module_delegate =
      std::make_unique<MissiveStorageModuleDelegateImpl>(
          base::BindPostTask(missive_client->origin_task_runner(),
                             base::BindRepeating(&MissiveClient::EnqueueRecord,
                                                 missive_client->GetWeakPtr())),
          base::BindPostTask(
              missive_client->origin_task_runner(),
              base::BindRepeating(&MissiveClient::Flush,
                                  missive_client->GetWeakPtr())));
  auto missive_storage_module =
      MissiveStorageModule::Create(std::move(missive_storage_module_delegate));
  if (!missive_storage_module) {
    std::move(cb).Run(Status(error::FAILED_PRECONDITION,
                             "Missive Storage Module failed to create"));
    return;
  }
  LOG(WARNING) << "Store reporting data by a Missive daemon";
  std::move(cb).Run(missive_storage_module);
  return;
}
#endif  // BUILDFLAG(IS_CHROMEOS)
}  // namespace reporting
