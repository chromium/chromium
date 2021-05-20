// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage_selector/storage_selector.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/reporting/encryption/encryption_module.h"
#include "components/reporting/storage/storage_module.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/storage/missive_storage_module.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace reporting {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Features settings for storage and uploader.
const base::Feature kUseMissiveDaemonFeature{StorageSelector::kUseMissiveDaemon,
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kProvideUploaderFeature{StorageSelector::kProvideUploader,
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
const char StorageSelector::kUseMissiveDaemon[] = "ConnectMissiveDaemon";
// static
const char StorageSelector::kProvideUploader[] = "ProvideUploader";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static
bool StorageSelector::is_uploader_required() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(kProvideUploaderFeature);
#else
  return true;  // Local storage must have an uploader.
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// static
void StorageSelector::CreateStorageModule(
    const base::FilePath& local_reporting_path,
    base::StringPiece verification_key,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>
        cb) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(kUseMissiveDaemonFeature)) {
    // Use Missive daemon as a storage.
    chromeos::MissiveClient* const missive_client =
        chromeos::MissiveClient::Get();
    if (!missive_client) {
      std::move(cb).Run(Status(
          error::FAILED_PRECONDITION,
          "Missive Client unavailable, probably has not been initialized"));
      return;
    }
    // Refer to the storage module.
    scoped_refptr<MissiveStorageModule> missive_module =
        missive_client->chromeos::MissiveClient::GetMissiveStorageModule();
    if (!missive_module) {
      std::move(cb).Run(
          Status(error::FAILED_PRECONDITION,
                 "Missive Client has not returned Storage Module"));
      return;
    }
    std::move(cb).Run(missive_module);
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Use Storage in a local file system.
  StorageModule::Create(
      StorageOptions()
          .set_directory(local_reporting_path)
          .set_signature_verification_public_key(verification_key),
      std::move(async_start_upload_cb), EncryptionModule::Create(),
      std::move(cb));
}

}  // namespace reporting
