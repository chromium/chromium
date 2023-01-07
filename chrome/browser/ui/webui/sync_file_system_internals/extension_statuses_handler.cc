// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/extension_statuses_handler.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

using sync_file_system::SyncFileSystemServiceFactory;
using sync_file_system::SyncServiceState;

namespace syncfs_internals {

namespace {

// TODO(crbug.com/989631): Break this and GetExtensionStatusesAsDictionary
// into a separate library, so the callbacks can use weak pointers to the
// calling instances. This will also break the dependency between
// ExtensionStatusesHandler and FileMetadataHandler.
void ConvertExtensionStatusToDictionary(
    const base::WeakPtr<extensions::ExtensionService>& extension_service,
    base::OnceCallback<void(const base::Value::List)> callback,
    const std::map<GURL, std::string>& status_map) {
  if (!extension_service) {
    std::move(callback).Run(base::Value::List());
    return;
  }

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(extension_service->profile());

  base::Value::List list;
  for (auto itr = status_map.begin(); itr != status_map.end(); ++itr) {
    std::string extension_id = itr->first.HostNoBrackets();

    // Join with human readable extension name.
    const extensions::Extension* extension =
        extension_registry->GetExtensionById(
            extension_id, extensions::ExtensionRegistry::EVERYTHING);
    if (!extension)
      continue;

    base::Value::Dict dict;
    dict.Set("extensionID", extension_id);
    dict.Set("extensionName", extension->name());
    dict.Set("status", itr->second);
    list.Append(std::move(dict));
  }

  std::move(callback).Run(std::move(list));
}

}  // namespace

ExtensionStatusesHandler::ExtensionStatusesHandler(Profile* profile)
    : profile_(profile) {}

ExtensionStatusesHandler::~ExtensionStatusesHandler() {}

void ExtensionStatusesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getExtensionStatuses",
      base::BindRepeating(&ExtensionStatusesHandler::HandleGetExtensionStatuses,
                          base::Unretained(this)));
}

// static
void ExtensionStatusesHandler::GetExtensionStatusesAsDictionary(
    Profile* profile,
    base::OnceCallback<void(const base::Value::List)> callback) {
  DCHECK(profile);

  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    std::move(callback).Run(base::Value::List());
    return;
  }

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service) {
    std::move(callback).Run(base::Value::List());
    return;
  }

  sync_service->GetExtensionStatusMap(base::BindOnce(
      &ConvertExtensionStatusToDictionary,
      extension_service->AsExtensionServiceWeakPtr(), std::move(callback)));
}

void ExtensionStatusesHandler::HandleGetExtensionStatuses(
    const base::Value::List& args) {
  AllowJavascript();
  GetExtensionStatusesAsDictionary(
      profile_,
      base::BindOnce(&ExtensionStatusesHandler::DidGetExtensionStatuses,
                     weak_ptr_factory_.GetWeakPtr(),
                     args[0].GetString() /* callback_id */));
}

void ExtensionStatusesHandler::DidGetExtensionStatuses(std::string callback_id,
                                                       base::Value::List list) {
  ResolveJavascriptCallback(base::Value(callback_id), list);
}

}  // namespace syncfs_internals
