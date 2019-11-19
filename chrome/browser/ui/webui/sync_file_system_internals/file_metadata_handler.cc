// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/file_metadata_handler.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/apps/platform_apps/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/ui/webui/sync_file_system_internals/extension_statuses_handler.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
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
      "getExtensions", base::BindRepeating(&FileMetadataHandler::GetExtensions,
                                           base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getFileMetadata",
      base::BindRepeating(&FileMetadataHandler::GetFileMetadata,
                          base::Unretained(this)));
}

void FileMetadataHandler::GetFileMetadata(
    const base::ListValue* args) {
  std::string extension_id;
  if (!args->GetString(0, &extension_id) || extension_id.empty()) {
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
  sync_service->DumpFiles(origin,
                          base::Bind(&FileMetadataHandler::DidGetFileMetadata,
                                     weak_factory_.GetWeakPtr()));
}

void FileMetadataHandler::GetExtensions(const base::ListValue* args) {
  DCHECK(args);
  ExtensionStatusesHandler::GetExtensionStatusesAsDictionary(
      profile_,
      base::Bind(&FileMetadataHandler::DidGetExtensions,
                 weak_factory_.GetWeakPtr()));
}

void FileMetadataHandler::DidGetExtensions(const base::ListValue& list) {
  web_ui()->CallJavascriptFunctionUnsafe("FileMetadata.onGetExtensions", list);
}

void FileMetadataHandler::DidGetFileMetadata(const base::ListValue& files) {
  web_ui()->CallJavascriptFunctionUnsafe("FileMetadata.onGetFileMetadata",
                                         files);
}

}  // namespace syncfs_internals
