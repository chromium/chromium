// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/file_metadata_handler.h"

#include <map>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/apps/platform_apps/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/ui/webui/sync_file_system_internals/extension_statuses_handler.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"

using sync_file_system::RemoteFileSyncService;
using sync_file_system::SyncFileSystemServiceFactory;
using sync_file_system::SyncServiceState;

namespace syncfs_internals {

FileMetadataHandler::FileMetadataHandler(Profile* profile)
    : profile_(profile) {}

FileMetadataHandler::~FileMetadataHandler() {}

void FileMetadataHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getExtensions",
      base::BindRepeating(&FileMetadataHandler::HandleGetExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getFileMetadata",
      base::BindRepeating(&FileMetadataHandler::HandleGetFileMetadata,
                          base::Unretained(this)));
}

void FileMetadataHandler::HandleGetFileMetadata(const base::Value::List& args) {
  AllowJavascript();
  std::string callback_id = args[0].GetString();
  std::string extension_id = args[1].GetString();
  if (extension_id.empty()) {
    LOG(WARNING) << "GetFileMetadata() Extension ID wasn't given";
    return;
  }

  // Extension ID from JS is just the host. Need to reformat it to chrome
  // extension type GURL.
  const GURL origin = extensions::Extension::GetBaseURLFromExtensionId(
      extension_id);

  // Get all metadata for the one specific origin.
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile_);
  if (!sync_service)
    return;

  content::StoragePartitionConfig storage_partition_config =
      extensions::util::GetStoragePartitionConfigForExtensionId(extension_id,
                                                                profile_);
  content::StoragePartition* storage_partition =
      profile_->GetStoragePartition(storage_partition_config);
  CHECK(storage_partition);

  sync_service->DumpFiles(
      storage_partition, origin,
      base::BindOnce(&FileMetadataHandler::DidGetFileMetadata,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void FileMetadataHandler::HandleGetExtensions(const base::Value::List& args) {
  AllowJavascript();
  ExtensionStatusesHandler::GetExtensionStatusesAsDictionary(
      profile_, base::BindOnce(&FileMetadataHandler::DidGetExtensions,
                               weak_factory_.GetWeakPtr(),
                               args[0].GetString() /* callback_id */));
}

void FileMetadataHandler::DidGetExtensions(std::string callback_id,
                                           base::Value::List list) {
  ResolveJavascriptCallback(base::Value(callback_id), list);
}

void FileMetadataHandler::DidGetFileMetadata(std::string callback_id,
                                             base::Value::List files) {
  ResolveJavascriptCallback(base::Value(callback_id), std::move(files));
}

}  // namespace syncfs_internals
