// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/extension_statuses_handler.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
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
    base::OnceCallback<void(const base::ListValue&)> callback,
    const std::map<GURL, std::string>& status_map) {
  if (!extension_service) {
    std::move(callback).Run(base::ListValue());
    return;
  }

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(extension_service->profile());

  base::ListValue list;
  for (auto itr = status_map.begin(); itr != status_map.end(); ++itr) {
    std::string extension_id = itr->first.HostNoBrackets();

    // Join with human readable extension name.
    const extensions::Extension* extension =
        extension_registry->GetExtensionById(
            extension_id, extensions::ExtensionRegistry::EVERYTHING);
    if (!extension)
      continue;

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    dict->SetString("extensionID", extension_id);
    dict->SetString("extensionName", extension->name());
    dict->SetString("status", itr->second);
    list.Append(std::move(dict));
  }

  std::move(callback).Run(list);
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
    base::OnceCallback<void(const base::ListValue&)> callback) {
  DCHECK(profile);

  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    std::move(callback).Run(base::ListValue());
    return;
  }

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service) {
    std::move(callback).Run(base::ListValue());
    return;
  }

  sync_service->GetExtensionStatusMap(
      base::BindOnce(&ConvertExtensionStatusToDictionary,
                     extension_service->AsWeakPtr(), std::move(callback)));
}

void ExtensionStatusesHandler::HandleGetExtensionStatuses(
    const base::ListValue* args) {
  AllowJavascript();
  DCHECK(args);
  GetExtensionStatusesAsDictionary(
      profile_,
      base::BindOnce(&ExtensionStatusesHandler::DidGetExtensionStatuses,
                     weak_ptr_factory_.GetWeakPtr(),
                     args->GetList()[0].GetString() /* callback_id */));
}

void ExtensionStatusesHandler::DidGetExtensionStatuses(
    std::string callback_id,
    const base::ListValue& list) {
  ResolveJavascriptCallback(base::Value(callback_id), list);
}

}  // namespace syncfs_internals
