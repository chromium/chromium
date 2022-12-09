// Copyright 2018 The Chromium Authors
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

using extensions::mojom::ManifestLocation;

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
    case extensions::Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION:
      return "TYPE_CHROMEOS_SYSTEM_EXTENSION";
    case extensions::Manifest::NUM_LOAD_TYPES:
      break;
  }
  NOTREACHED();
  return "";
}

const char* LocationToString(ManifestLocation loc) {
  switch (loc) {
    case ManifestLocation::kInvalidLocation:
      return "INVALID_LOCATION";
    case ManifestLocation::kInternal:
      return "INTERNAL";
    case ManifestLocation::kExternalPref:
      return "EXTERNAL_PREF";
    case ManifestLocation::kExternalRegistry:
      return "EXTERNAL_REGISTRY";
    case ManifestLocation::kUnpacked:
      return "UNPACKED";
    case ManifestLocation::kComponent:
      return "COMPONENT";
    case ManifestLocation::kExternalPrefDownload:
      return "EXTERNAL_PREF_DOWNLOAD";
    case ManifestLocation::kExternalPolicyDownload:
      return "EXTERNAL_POLICY_DOWNLOAD";
    case ManifestLocation::kCommandLine:
      return "COMMAND_LINE";
    case ManifestLocation::kExternalPolicy:
      return "EXTERNAL_POLICY";
    case ManifestLocation::kExternalComponent:
      return "EXTERNAL_COMPONENT";
  }
  NOTREACHED();
  return "";
}

base::Value::List CreationFlagsToList(int creation_flags) {
  base::Value::List flags_value;
  if (creation_flags == extensions::Extension::NO_FLAGS)
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

base::Value::List DisableReasonsToList(int disable_reasons) {
  base::Value::List disable_reasons_value;
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

base::Value::Dict FormatKeepaliveData(
    extensions::ProcessManager* process_manager,
    const extensions::Extension* extension) {
  base::Value::Dict keepalive_data;
  keepalive_data.Set(kCountKey,
                     process_manager->GetLazyKeepaliveCount(extension));
  const extensions::ProcessManager::ActivitiesMultiset activities =
      process_manager->GetLazyKeepaliveActivities(extension);
  base::Value::List activities_data;
  for (const auto& activity : activities) {
    base::Value::Dict activities_entry;
    activities_entry.Set(kTypeKey,
                         extensions::Activity::ToString(activity.first));
    activities_entry.Set(kExtraDataKey, activity.second);
    activities_data.Append(std::move(activities_entry));
  }
  keepalive_data.Set(kActivitesKey, std::move(activities_data));
  return keepalive_data;
}

// Formats API and Manifest permissions, which can have details that we add as a
// dictionary rather than just the string name
template <typename T>
base::Value::List FormatDetailedPermissionSet(const T& permissions) {
  base::Value::List value_list;
  for (const auto& permission : permissions) {
    if (auto detail = permission->ToValue()) {
      base::Value::Dict tmp;
      tmp.Set(permission->name(),
              base::Value::FromUniquePtrValue(std::move(detail)));
      value_list.Append(std::move(tmp));
    } else {
      value_list.Append(permission->name());
    }
  }
  return value_list;
}

base::Value::Dict FormatPermissionSet(
    const extensions::PermissionSet& permission_set) {
  base::Value::Dict value;

  value.Set(kPermissionsExplicitHostsKey,
            permission_set.explicit_hosts().ToValue());
  value.Set(kPermissionsScriptableHostsKey,
            permission_set.scriptable_hosts().ToValue());
  value.Set(kPermissionsManifestKey,
            FormatDetailedPermissionSet(permission_set.manifest_permissions()));
  value.Set(kPermissionsApiKey,
            FormatDetailedPermissionSet(permission_set.apis()));

  return value;
}

base::Value::Dict FormatPermissionsData(
    const extensions::Extension& extension) {
  const extensions::PermissionsData& permissions =
      *extension.permissions_data();
  base::Value::Dict permissions_data;

  const extensions::PermissionSet& active_permissions =
      permissions.active_permissions();
  permissions_data.Set(kPermissionsActiveKey,
                       FormatPermissionSet(active_permissions));

  const extensions::PermissionSet& withheld_permissions =
      permissions.withheld_permissions();
  permissions_data.Set(kPermissionsWithheldKey,
                       FormatPermissionSet(withheld_permissions));

  base::Value::Dict tab_specific;
  for (const auto& tab : permissions.tab_specific_permissions()) {
    tab_specific.Set(base::NumberToString(tab.first),
                     FormatPermissionSet(*tab.second));
  }
  permissions_data.Set(kPermissionsTabSpecificKey, std::move(tab_specific));

  const extensions::PermissionSet& optional_permissions =
      extensions::PermissionsParser::GetOptionalPermissions(&extension);
  permissions_data.Set(kPermissionsOptionalKey,
                       FormatPermissionSet(optional_permissions));

  return permissions_data;
}

void AddEventListenerData(extensions::EventRouter* event_router,
                          base::Value::List* data) {
  // A map of extension ID to the listener data for that extension,
  // which is of type LIST of DICTIONARY.
  std::unordered_map<base::StringPiece, base::Value::List,
                     base::StringPieceHash>
      listeners_map;

  // Build the map of extension IDs to the list of events.
  for (const auto& entry : event_router->listeners().listeners()) {
    for (const auto& listener_entry : entry.second) {
      auto& listeners_list = listeners_map[listener_entry->extension_id()];
      // The data for each listener is a dictionary.
      base::Value::Dict listener_data;
      listener_data.Set(kEventNameKey, listener_entry->event_name());
      listener_data.Set(kIsForServiceWorkerKey,
                        listener_entry->is_for_service_worker());
      listener_data.Set(kIsLazyKey, listener_entry->IsLazy());
      listener_data.Set(kListenerUrlKey, listener_entry->listener_url().spec());
      // Add the filter if one exists.
      const base::Value::Dict* const filter = listener_entry->filter();
      if (filter) {
        listener_data.Set(kFilterKey, filter->Clone());
      }
      listeners_list.Append(std::move(listener_data));
    }
  }

  // Move all of the entries from the map into the output data.
  for (auto& output_entry : *data) {
    const base::Value* const value =
        output_entry.GetDict().Find(kInternalsIdKey);
    CHECK(value && value->is_string());
    const auto it = listeners_map.find(value->GetString());
    base::Value::Dict event_listeners;
    if (it == listeners_map.end()) {
      // We didn't find any events, so initialize an empty dictionary.
      event_listeners.Set(kCountKey, 0);
      event_listeners.Set(kListenersKey, base::Value::List());
    } else {
      // Set the count and the events values.
      event_listeners.Set(kCountKey,
                          base::checked_cast<int>(it->second.size()));
      event_listeners.Set(kListenersKey, std::move(it->second));
    }
    output_entry.GetDict().Set(kEventsListenersKey, std::move(event_listeners));
  }
}

}  // namespace

ExtensionsInternalsSource::ExtensionsInternalsSource(Profile* profile)
    : profile_(profile) {}

ExtensionsInternalsSource::~ExtensionsInternalsSource() = default;

std::string ExtensionsInternalsSource::GetSource() {
  return chrome::kChromeUIExtensionsInternalsHost;
}

std::string ExtensionsInternalsSource::GetMimeType(const GURL& url) {
  return "text/plain";
}

void ExtensionsInternalsSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  std::string json = WriteToString();
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(json)));
}

std::string ExtensionsInternalsSource::WriteToString() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<extensions::ExtensionSet> extensions =
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet();
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(profile_);
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  base::Value::List data;
  for (const auto& extension : *extensions) {
    base::Value::Dict extension_data;
    extension_data.Set(kInternalsIdKey, extension->id());
    extension_data.Set(kInternalsCreationFlagsKey,
                       CreationFlagsToList(extension->creation_flags()));
    extension_data.Set(
        kInternalsDisableReasonsKey,
        DisableReasonsToList(prefs->GetDisableReasons(extension->id())));
    extension_data.Set(kKeepaliveKey,
                       FormatKeepaliveData(process_manager, extension.get()));
    extension_data.Set(kLocationKey, LocationToString(extension->location()));
    extension_data.Set(kManifestVersionKey, extension->manifest_version());
    extension_data.Set(kInternalsNameKey, extension->name());
    extension_data.Set(kPathKey, extension->path().LossyDisplayName());
    extension_data.Set(kTypeKey, TypeToString(extension->GetType()));
    extension_data.Set(kInternalsVersionKey, extension->GetVersionForDisplay());
    extension_data.Set(kPermissionsKey, FormatPermissionsData(*extension));
    data.Append(std::move(extension_data));
  }

  // Aggregate and add the data for the registered event listeners.
  AddEventListenerData(extensions::EventRouter::Get(profile_), &data);

  std::string json;
  base::JSONWriter::WriteWithOptions(
      data, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);

  return json;
}
