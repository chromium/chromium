// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_internals_source.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/activity.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace {

const char* TypeToString(extensions::Manifest::Type type) {
  switch (type) {
    case extensions::Manifest::TYPE_UNKNOWN:
      return "TYPE_UNKNOWN";
    case extensions::Manifest::TYPE_EXTENSION:
      return "TYPE_EXTENSION";
    case extensions::Manifest::TYPE_THEME:
      return "TYPE_THEME";
    case extensions::Manifest::TYPE_USER_SCRIPT:
      return "TYPE_USER_SCRIPT";
    case extensions::Manifest::TYPE_HOSTED_APP:
      return "TYPE_HOSTED_APP";
    case extensions::Manifest::TYPE_LEGACY_PACKAGED_APP:
      return "TYPE_LEGACY_PACKAGED_APP";
    case extensions::Manifest::TYPE_PLATFORM_APP:
      return "TYPE_PLATFORM_APP";
    case extensions::Manifest::TYPE_SHARED_MODULE:
      return "TYPE_SHARED_MODULE";
    case extensions::Manifest::TYPE_LOGIN_SCREEN_EXTENSION:
      return "TYPE_LOGIN_SCREEN_EXTENSION";
    case extensions::Manifest::NUM_LOAD_TYPES:
      break;
  }
  NOTREACHED();
  return "";
}

const char* LocationToString(extensions::Manifest::Location loc) {
  switch (loc) {
    case extensions::Manifest::INVALID_LOCATION:
      return "INVALID_LOCATION";
    case extensions::Manifest::INTERNAL:
      return "INTERNAL";
    case extensions::Manifest::EXTERNAL_PREF:
      return "EXTERNAL_PREF";
    case extensions::Manifest::EXTERNAL_REGISTRY:
      return "EXTERNAL_REGISTRY";
    case extensions::Manifest::UNPACKED:
      return "UNPACKED";
    case extensions::Manifest::COMPONENT:
      return "COMPONENT";
    case extensions::Manifest::EXTERNAL_PREF_DOWNLOAD:
      return "EXTERNAL_PREF_DOWNLOAD";
    case extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD:
      return "EXTERNAL_POLICY_DOWNLOAD";
    case extensions::Manifest::COMMAND_LINE:
      return "COMMAND_LINE";
    case extensions::Manifest::EXTERNAL_POLICY:
      return "EXTERNAL_POLICY";
    case extensions::Manifest::EXTERNAL_COMPONENT:
      return "EXTERNAL_COMPONENT";
    case extensions::Manifest::NUM_LOCATIONS:
      break;
  }
  NOTREACHED();
  return "";
}

base::Value CreationFlagsToList(int creation_flags) {
  base::Value flags_value(base::Value::Type::LIST);
  if (creation_flags & extensions::Extension::NO_FLAGS)
    flags_value.Append("NO_FLAGS");
  if (creation_flags & extensions::Extension::REQUIRE_KEY)
    flags_value.Append("REQUIRE_KEY");
  if (creation_flags & extensions::Extension::REQUIRE_MODERN_MANIFEST_VERSION)
    flags_value.Append("REQUIRE_MODERN_MANIFEST_VERSION");
  if (creation_flags & extensions::Extension::ALLOW_FILE_ACCESS)
    flags_value.Append("ALLOW_FILE_ACCESS");
  if (creation_flags & extensions::Extension::FROM_WEBSTORE)
    flags_value.Append("FROM_WEBSTORE");
  if (creation_flags & extensions::Extension::FROM_BOOKMARK)
    flags_value.Append("FROM_BOOKMARK");
  if (creation_flags & extensions::Extension::FOLLOW_SYMLINKS_ANYWHERE)
    flags_value.Append("FOLLOW_SYMLINKS_ANYWHERE");
  if (creation_flags & extensions::Extension::ERROR_ON_PRIVATE_KEY)
    flags_value.Append("ERROR_ON_PRIVATE_KEY");
  if (creation_flags & extensions::Extension::WAS_INSTALLED_BY_DEFAULT)
    flags_value.Append("WAS_INSTALLED_BY_DEFAULT");
  if (creation_flags & extensions::Extension::REQUIRE_PERMISSIONS_CONSENT)
    flags_value.Append("REQUIRE_PERMISSIONS_CONSENT");
  if (creation_flags & extensions::Extension::IS_EPHEMERAL)
    flags_value.Append("IS_EPHEMERAL");
  if (creation_flags & extensions::Extension::WAS_INSTALLED_BY_OEM)
    flags_value.Append("WAS_INSTALLED_BY_OEM");
  if (creation_flags & extensions::Extension::MAY_BE_UNTRUSTED)
    flags_value.Append("MAY_BE_UNTRUSTED");
  if (creation_flags & extensions::Extension::WITHHOLD_PERMISSIONS)
    flags_value.Append("WITHHOLD_PERMISSIONS");
  return flags_value;
}

base::Value DisableReasonsToList(int disable_reasons) {
  base::Value disable_reasons_value(base::Value::Type::LIST);
  if (disable_reasons &
      extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE) {
    disable_reasons_value.Append("DISABLE_PERMISSIONS_INCREASE");
  }
  if (disable_reasons & extensions::disable_reason::DISABLE_RELOAD)
    disable_reasons_value.Append("DISABLE_RELOAD");
  if (disable_reasons &
      extensions::disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT) {
    disable_reasons_value.Append("DISABLE_UNSUPPORTED_REQUIREMENT");
  }
  if (disable_reasons & extensions::disable_reason::DISABLE_SIDELOAD_WIPEOUT)
    disable_reasons_value.Append("DISABLE_SIDELOAD_WIPEOUT");
  if (disable_reasons & extensions::disable_reason::DISABLE_NOT_VERIFIED)
    disable_reasons_value.Append("DISABLE_NOT_VERIFIED");
  if (disable_reasons & extensions::disable_reason::DISABLE_GREYLIST)
    disable_reasons_value.Append("DISABLE_GREYLIST");
  if (disable_reasons & extensions::disable_reason::DISABLE_CORRUPTED)
    disable_reasons_value.Append("DISABLE_CORRUPTED");
  if (disable_reasons & extensions::disable_reason::DISABLE_REMOTE_INSTALL)
    disable_reasons_value.Append("DISABLE_REMOTE_INSTALL");
  if (disable_reasons & extensions::disable_reason::DISABLE_EXTERNAL_EXTENSION)
    disable_reasons_value.Append("DISABLE_EXTERNAL_EXTENSION");
  if (disable_reasons &
      extensions::disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY) {
    disable_reasons_value.Append("DISABLE_UPDATE_REQUIRED_BY_POLICY");
  }
  if (disable_reasons &
      extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED) {
    disable_reasons_value.Append("DISABLE_CUSTODIAN_APPROVAL_REQUIRED");
  }
  if (disable_reasons & extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY)
    disable_reasons_value.Append("DISABLE_BLOCKED_BY_POLICY");
  if (disable_reasons &
      extensions::disable_reason::DISABLE_REMOTELY_FOR_MALWARE) {
    disable_reasons_value.Append("DISABLE_REMOTELY_FOR_MALWARE");
  }
  return disable_reasons_value;
}
// The JSON we generate looks like this:
// Note:
// - tab_specific permissions can have 0 or more DICT entries with each tab id
// pointing to the api, explicit_host, manifest and scriptable_host permission
// lists.
// - In some cases manifest or api permissions rather than just being a STRING
// can be a DICT with the name keying a more complex object with detailed
// information. This is the case for subclasses of ManifestPermission and
// APIPermission which override the ToValue function.
//
// [ {
//    "creation_flags": [ "ALLOW_FILE_ACCESS", "FROM_WEBSTORE" ],
//    "disable_reasons": ["DISABLE_USER_ACTION"],
//    "event_listeners": {
//       "count": 2,
//       "events": [ {
//          "name": "runtime.onInstalled"
//       }, {
//          "name": "runtime.onSuspend"
//       } ]
//    },
//    "id": "bhloflhklmhfpedakmangadcdofhnnoh",
//    "keepalive": {
//       "activities": [ {
//          "extra_data": "render-frame",
//          "type": "PROCESS_MANAGER"
//       } ],
//       "count": 1
//    },
//    "location": "INTERNAL",
//    "manifest_version": 2,
//    "name": "Earth View from Google Earth",
//    "path": "/user/Extensions/bhloflhklmhfpedakmangadcdofhnnoh/2.18.5_0",
//    "permissions": {
//       "active": {
//          "api": [ ],
//          "explicit_hosts": [ ],
//          "manifest": [ ],
//          "scriptable_hosts": [ ]
//       },
//       "optional": {
//          "api": [ ],
//          "explicit_hosts": [ ],
//          "manifest": [ ],
//          "scriptable_hosts": [ ]
//       },
//       "tab_specific": {
//          "4": {
//             "api": [ ],
//             "explicit_hosts": [ ],
//             "manifest": [ ],
//             "scriptable_hosts": [ ]
//          }
//       },
//       "withheld": {
//          "api": [ ],
//          "explicit_hosts": [ ],
//          "manifest": [ ],
//          "scriptable_hosts": [ ]
//       },
//    "type": "TYPE_EXTENSION",
//    "version": "2.18.5"
// } ]
//
// Which is:
//
// LIST
//  DICT
//    "creation_flags": LIST
//      STRING
//    "disable_reasons": LIST
//      STRING
//    "event_listeners": DICT
//      "count": INT
//      "listeners": LIST
//        DICT
//          "event_name": STRING
//          "filter": DICT
//          "is_for_service_worker": STRING
//          "is_lazy": STRING
//          "url": STRING
//    "id": STRING
//    "keepalive": DICT
//      "activities": LIST
//        DICT
//          "extra_data": STRING
//          "type": STRING
//      "count": INT
//    "location": STRING
//    "manifest_version": INT
//    "name": STRING
//    "path": STRING
//    "permissions": DICT
//      "active": DICT
//        "api": LIST
//          STRING
//          (see note above for edge cases on all "api" and "manfest" entries)
//        "explicit_hosts": LIST
//          STRING
//        "manifest": LIST
//          STRING
//        "scriptable_hosts": LIST
//          STRING
//      "optional": DICT
//        "api": LIST
//          STRING
//        "explicit_hosts": LIST
//          STRING
//        "manifest": LIST
//          STRING
//        "scriptable_hosts": LIST
//          STRING
//      "tab_specific": DICT
//        (see note above for details)
//      "withheld": DICT
//        "api": LIST
//          STRING
//        "explicit_hosts": LIST
//          STRING
//        "manifest": LIST
//          STRING
//        "scriptable_hosts": LIST
//          STRING
//    "type": STRING
//    "version": STRING

constexpr base::StringPiece kActivitesKey = "activites";
constexpr base::StringPiece kCountKey = "count";
constexpr base::StringPiece kEventNameKey = "event_name";
constexpr base::StringPiece kEventsListenersKey = "event_listeners";
constexpr base::StringPiece kExtraDataKey = "extra_data";
constexpr base::StringPiece kFilterKey = "filter";
constexpr base::StringPiece kInternalsCreationFlagsKey = "creation_flags";
constexpr base::StringPiece kInternalsDisableReasonsKey = "disable_reasons";
constexpr base::StringPiece kInternalsIdKey = "id";
constexpr base::StringPiece kInternalsNameKey = "name";
constexpr base::StringPiece kInternalsVersionKey = "version";
constexpr base::StringPiece kIsForServiceWorkerKey = "is_for_service_worker";
constexpr base::StringPiece kIsLazyKey = "is_lazy";
constexpr base::StringPiece kListenersKey = "listeners";
constexpr base::StringPiece kKeepaliveKey = "keepalive";
constexpr base::StringPiece kListenerUrlKey = "url";
constexpr base::StringPiece kLocationKey = "location";
constexpr base::StringPiece kManifestVersionKey = "manifest_version";
constexpr base::StringPiece kPathKey = "path";
constexpr base::StringPiece kPermissionsKey = "permissions";
constexpr base::StringPiece kPermissionsActiveKey = "active";
constexpr base::StringPiece kPermissionsOptionalKey = "optional";
constexpr base::StringPiece kPermissionsTabSpecificKey = "tab_specific";
constexpr base::StringPiece kPermissionsWithheldKey = "withheld";
constexpr base::StringPiece kPermissionsApiKey = "api";
constexpr base::StringPiece kPermissionsManifestKey = "manifest";
constexpr base::StringPiece kPermissionsExplicitHostsKey = "explicit_hosts";
constexpr base::StringPiece kPermissionsScriptableHostsKey = "scriptable_hosts";
constexpr base::StringPiece kTypeKey = "type";

base::Value FormatKeepaliveData(extensions::ProcessManager* process_manager,
                                const extensions::Extension* extension) {
  base::Value keepalive_data(base::Value::Type::DICTIONARY);
  keepalive_data.SetKey(
      kCountKey,
      base::Value(process_manager->GetLazyKeepaliveCount(extension)));
  const extensions::ProcessManager::ActivitiesMultiset activities =
      process_manager->GetLazyKeepaliveActivities(extension);
  base::Value activities_data(base::Value::Type::LIST);
  for (const auto& activity : activities) {
    base::Value activities_entry(base::Value::Type::DICTIONARY);
    activities_entry.SetKey(
        kTypeKey, base::Value(extensions::Activity::ToString(activity.first)));
    activities_entry.SetKey(kExtraDataKey, base::Value(activity.second));
    activities_data.Append(std::move(activities_entry));
  }
  keepalive_data.SetKey(kActivitesKey, std::move(activities_data));
  return keepalive_data;
}

// Formats API and Manifest permissions, which can have details that we add as a
// dictionary rather than just the string name
template <typename T>
base::Value FormatDetailedPermissionSet(const T& permissions) {
  base::Value value_list(base::Value::Type::LIST);
  for (const auto& permission : permissions) {
    std::unique_ptr<base::Value> detail(permission->ToValue());
    if (detail) {
      base::Value tmp(base::Value::Type::DICTIONARY);
      tmp.SetKey(permission->name(),
                 base::Value::FromUniquePtrValue(std::move(detail)));
      value_list.Append(std::move(tmp));
    } else {
      value_list.Append(base::Value(permission->name()));
    }
  }
  return value_list;
}

base::Value FormatPermissionSet(
    const extensions::PermissionSet& permission_set) {
  base::Value value(base::Value::Type::DICTIONARY);

  value.SetKey(kPermissionsExplicitHostsKey,
               base::Value::FromUniquePtrValue(
                   permission_set.explicit_hosts().ToValue()));
  value.SetKey(kPermissionsScriptableHostsKey,
               base::Value::FromUniquePtrValue(
                   permission_set.scriptable_hosts().ToValue()));
  value.SetKey(
      kPermissionsManifestKey,
      FormatDetailedPermissionSet(permission_set.manifest_permissions()));
  value.SetKey(kPermissionsApiKey,
               FormatDetailedPermissionSet(permission_set.apis()));

  return value;
}

base::Value FormatPermissionsData(const extensions::Extension& extension) {
  const extensions::PermissionsData& permissions =
      *extension.permissions_data();
  base::Value permissions_data(base::Value::Type::DICTIONARY);

  const extensions::PermissionSet& active_permissions =
      permissions.active_permissions();
  permissions_data.SetKey(kPermissionsActiveKey,
                          FormatPermissionSet(active_permissions));

  const extensions::PermissionSet& withheld_permissions =
      permissions.withheld_permissions();
  permissions_data.SetKey(kPermissionsWithheldKey,
                          FormatPermissionSet(withheld_permissions));

  base::Value tab_specific(base::Value::Type::DICTIONARY);
  for (const auto& tab : permissions.tab_specific_permissions()) {
    tab_specific.SetKey(base::NumberToString(tab.first),
                        FormatPermissionSet(*tab.second));
  }
  permissions_data.SetKey(kPermissionsTabSpecificKey, std::move(tab_specific));

  const extensions::PermissionSet& optional_permissions =
      extensions::PermissionsParser::GetOptionalPermissions(&extension);
  permissions_data.SetKey(kPermissionsOptionalKey,
                          FormatPermissionSet(optional_permissions));

  return permissions_data;
}

void AddEventListenerData(extensions::EventRouter* event_router,
                          base::Value* data) {
  CHECK(data->is_list());
  // A map of extension ID to the listener data for that extension,
  // which is of type LIST of DICTIONARY.
  std::unordered_map<base::StringPiece, base::Value, base::StringPieceHash>
      listeners_map;

  // Build the map of extension IDs to the list of events.
  for (const auto& entry : event_router->listeners().listeners()) {
    for (const auto& listener_entry : entry.second) {
      auto& listeners_list = listeners_map[listener_entry->extension_id()];
      if (listeners_list.is_none()) {
        // Not there, so make it a LIST.
        listeners_list = base::Value(base::Value::Type::LIST);
      }
      // The data for each listener is a dictionary.
      base::Value listener_data(base::Value::Type::DICTIONARY);
      listener_data.SetKey(kEventNameKey,
                           base::Value(listener_entry->event_name()));
      listener_data.SetKey(
          kIsForServiceWorkerKey,
          base::Value(listener_entry->is_for_service_worker()));
      listener_data.SetKey(kIsLazyKey, base::Value(listener_entry->IsLazy()));
      listener_data.SetKey(kListenerUrlKey,
                           base::Value(listener_entry->listener_url().spec()));
      // Add the filter if one exists.
      base::Value* const filter = listener_entry->filter();
      if (filter) {
        listener_data.SetKey(kFilterKey, filter->Clone());
      }
      listeners_list.Append(std::move(listener_data));
    }
  }

  // Move all of the entries from the map into the output data.
  for (auto& output_entry : data->GetList()) {
    const base::Value* const value = output_entry.FindKey(kInternalsIdKey);
    CHECK(value && value->is_string());
    const auto it = listeners_map.find(value->GetString());
    base::Value event_listeners(base::Value::Type::DICTIONARY);
    if (it == listeners_map.end()) {
      // We didn't find any events, so initialize an empty dictionary.
      event_listeners.SetKey(kCountKey, base::Value(0));
      event_listeners.SetKey(kListenersKey,
                             base::Value(base::Value::Type::LIST));
    } else {
      // Set the count and the events values.
      event_listeners.SetKey(
          kCountKey,
          base::Value(base::checked_cast<int>(it->second.GetList().size())));
      event_listeners.SetKey(kListenersKey, std::move(it->second));
    }
    output_entry.SetKey(kEventsListenersKey, std::move(event_listeners));
  }
}

}  // namespace

ExtensionsInternalsSource::ExtensionsInternalsSource(Profile* profile)
    : profile_(profile) {}

ExtensionsInternalsSource::~ExtensionsInternalsSource() = default;

std::string ExtensionsInternalsSource::GetSource() {
  return chrome::kChromeUIExtensionsInternalsHost;
}

std::string ExtensionsInternalsSource::GetMimeType(const std::string& path) {
  return "text/plain";
}

void ExtensionsInternalsSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  std::string json = WriteToString();
  std::move(callback).Run(base::RefCountedString::TakeString(&json));
}

std::string ExtensionsInternalsSource::WriteToString() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<extensions::ExtensionSet> extensions =
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet();
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(profile_);
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  base::Value data(base::Value::Type::LIST);
  for (const auto& extension : *extensions) {
    base::Value extension_data(base::Value::Type::DICTIONARY);
    extension_data.SetKey(kInternalsIdKey, base::Value(extension->id()));
    extension_data.SetKey(kInternalsCreationFlagsKey,
                          CreationFlagsToList(extension->creation_flags()));
    extension_data.SetKey(
        kInternalsDisableReasonsKey,
        DisableReasonsToList(prefs->GetDisableReasons(extension->id())));
    extension_data.SetKey(
        kKeepaliveKey, FormatKeepaliveData(process_manager, extension.get()));
    extension_data.SetKey(kLocationKey,
                          base::Value(LocationToString(extension->location())));
    extension_data.SetKey(kManifestVersionKey,
                          base::Value(extension->manifest_version()));
    extension_data.SetKey(kInternalsNameKey, base::Value(extension->name()));
    extension_data.SetKey(kPathKey,
                          base::Value(extension->path().LossyDisplayName()));
    extension_data.SetKey(kTypeKey,
                          base::Value(TypeToString(extension->GetType())));
    extension_data.SetKey(kInternalsVersionKey,
                          base::Value(extension->GetVersionForDisplay()));
    extension_data.SetKey(kPermissionsKey, FormatPermissionsData(*extension));
    data.Append(std::move(extension_data));
  }

  // Aggregate and add the data for the registered event listeners.
  AddEventListenerData(extensions::EventRouter::Get(profile_), &data);

  std::string json;
  base::JSONWriter::WriteWithOptions(
      data, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);

  return json;
}
