// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/dump_database_handler.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

using sync_file_system::SyncFileSystemServiceFactory;

namespace syncfs_internals {

DumpDatabaseHandler::DumpDatabaseHandler(Profile* profile)
    : profile_(profile) {}
DumpDatabaseHandler::~DumpDatabaseHandler() {}

void DumpDatabaseHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDatabaseDump",
      base::BindRepeating(&DumpDatabaseHandler::HandleGetDatabaseDump,
                          weak_factory_.GetWeakPtr()));
}

void DumpDatabaseHandler::HandleGetDatabaseDump(const base::Value::List& args) {
  AllowJavascript();
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile_);
  if (sync_service) {
    sync_service->DumpDatabase(base::BindOnce(
        &DumpDatabaseHandler::DidGetDatabaseDump, weak_factory_.GetWeakPtr(),
        args[0].GetString() /* callback_id */));
  }
}

void DumpDatabaseHandler::DidGetDatabaseDump(std::string callback_id,
                                             base::Value::List list) {
  ResolveJavascriptCallback(base::Value(callback_id), std::move(list));
}

}  // namespace syncfs_internals
