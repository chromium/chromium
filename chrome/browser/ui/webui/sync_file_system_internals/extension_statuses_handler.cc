// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/extension_statuses_handler.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

using sync_file_system::SyncFileSystemServiceFactory;
using sync_file_system::SyncServiceState;

namespace syncfs_internals {

namespace {

void ConvertExtensionStatusToDictionary(
    const base::WeakPtr<extensions::ExtensionService>& extension_service,
    const base::Callback<void(const base::ListValue&)>& callback,
    const std::map<GURL, std::string>& status_map) {
  if (!extension_service) {
    callback.Run(base::ListValue());
    return;
  }

  base::ListValue list;
  for (auto itr = status_map.begin(); itr != status_map.end(); ++itr) {
    std::string extension_id = itr->first.HostNoBrackets();

    // Join with human readable extension name.
    const extensions::Extension* extension =
        extension_service->GetExtensionById(extension_id, true);
    if (!extension)
      continue;

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    dict->SetString("extensionID", extension_id);
    dict->SetString("extensionName", extension->name());
    dict->SetString("status", itr->second);
    list.Append(std::move(dict));
  }

  callback.Run(list);
}

}  // namespace

ExtensionStatusesHandler::ExtensionStatusesHandler(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {}

ExtensionStatusesHandler::~ExtensionStatusesHandler() {}

void ExtensionStatusesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getExtensionStatuses",
      base::BindRepeating(&ExtensionStatusesHandler::GetExtensionStatuses,
                          base::Unretained(this)));
}

// static
void ExtensionStatusesHandler::GetExtensionStatusesAsDictionary(
    Profile* profile,
    const base::Callback<void(const base::ListValue&)>& callback) {
  DCHECK(profile);

  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    callback.Run(base::ListValue());
    return;
  }

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service) {
    callback.Run(base::ListValue());
    return;
  }

  sync_service->GetExtensionStatusMap(base::Bind(
      &ConvertExtensionStatusToDictionary,
      extension_service->AsWeakPtr(), callback));
}

void ExtensionStatusesHandler::GetExtensionStatuses(
    const base::ListValue* args) {
  DCHECK(args);
  GetExtensionStatusesAsDictionary(
      profile_,
      base::Bind(&ExtensionStatusesHandler::DidGetExtensionStatuses,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionStatusesHandler::DidGetExtensionStatuses(
    const base::ListValue& list) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "ExtensionStatuses.onGetExtensionStatuses", list);
}

}  // namespace syncfs_internals
