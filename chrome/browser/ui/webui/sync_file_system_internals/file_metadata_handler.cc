// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/file_metadata_handler.h"

#include <map>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
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
      "getExtensions",
      base::BindRepeating(&FileMetadataHandler::HandleGetExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getFileMetadata",
      base::BindRepeating(&FileMetadataHandler::HandleGetFileMetadata,
                          base::Unretained(this)));
}

void FileMetadataHandler::HandleGetFileMetadata(const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id = args->GetList()[0].GetString();
  std::string extension_id = args->GetList()[1].GetString();
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
  sync_service->DumpFiles(
      origin, base::BindOnce(&FileMetadataHandler::DidGetFileMetadata,
                             weak_factory_.GetWeakPtr(), callback_id));
}

void FileMetadataHandler::HandleGetExtensions(const base::ListValue* args) {
  AllowJavascript();
  DCHECK(args);
  ExtensionStatusesHandler::GetExtensionStatusesAsDictionary(
      profile_,
      base::BindOnce(&FileMetadataHandler::DidGetExtensions,
                     weak_factory_.GetWeakPtr(),
                     args->GetList()[0].GetString() /* callback_id */));
}

void FileMetadataHandler::DidGetExtensions(std::string callback_id,
                                           const base::ListValue& list) {
  ResolveJavascriptCallback(base::Value(callback_id), list);
}

void FileMetadataHandler::DidGetFileMetadata(std::string callback_id,
                                             const base::ListValue& files) {
  ResolveJavascriptCallback(base::Value(callback_id), files);
}

}  // namespace syncfs_internals
