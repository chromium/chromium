// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module.h"
#include "components/reporting/storage/storage_module.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/storage/missive_storage_module.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

namespace reporting {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Features settings for storage and uploader.
// Use `missived` by all browsers.
const base::Feature kUseMissiveDaemonFeature{StorageSelector::kUseMissiveDaemon,
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Receive `missived` uploads by ASH/primary browser only.
const base::Feature kProvideUploaderFeature {
  StorageSelector::kProvideUploader,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // BUILDFLAG(IS_CHROMEOS_LACROS)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
// static
const char StorageSelector::kUseMissiveDaemon[] = "ConnectMissiveDaemon";
// static
const char StorageSelector::kProvideUploader[] = "ProvideUploader";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

// static
bool StorageSelector::is_uploader_required() {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return base::FeatureList::IsEnabled(kProvideUploaderFeature);
#else   // Not ChromeOS
  return true;  // Local storage must have an uploader.
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

// static
bool StorageSelector::is_use_missive() {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return base::FeatureList::IsEnabled(kUseMissiveDaemonFeature);
#else   // Not ChromeOS
  return false;  // Use Local storage.
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

// static
void StorageSelector::CreateStorageModule(
    const base::FilePath& local_reporting_path,
    base::StringPiece verification_key,
    CompressionInformation::CompressionAlgorithm compression_algorithm,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>
        cb) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
    LOG(WARNING) << "Store reporting data by a Missive daemon";
    std::move(cb).Run(missive_module);
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  // Use Storage in a local file system.
  LOG(WARNING) << "Store reporting data locally";
  StorageModule::Create(
      StorageOptions()
          .set_directory(local_reporting_path)
          .set_signature_verification_public_key(verification_key),
      std::move(async_start_upload_cb), EncryptionModule::Create(),
      CompressionModule::Create(512, compression_algorithm), std::move(cb));
}

}  // namespace reporting
