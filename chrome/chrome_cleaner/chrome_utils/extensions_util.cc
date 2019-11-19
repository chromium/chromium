// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/chrome_utils/extensions_util.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/chrome_utils/chrome_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/registry.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/parsers/json_parser/json_splicer.h"
#include "chrome/chrome_cleaner/parsers/parser_utils/parse_tasks_remaining_counter.h"

using base::WaitableEvent;

namespace chrome_cleaner {
namespace {

const int kExtensionIdLength = 32;

// TODO(joenotcharles): Use RegKeyPath instead.
struct RegistryKey {
  HKEY hkey;
  const wchar_t* path;
};

const RegistryKey extension_forcelist_keys[] = {
    {HKEY_LOCAL_MACHINE, kChromePoliciesForcelistKeyPath},
    {HKEY_CURRENT_USER, kChromePoliciesForcelistKeyPath}};

const RegistryKey extension_settings_keys[] = {
    {HKEY_LOCAL_MACHINE, kChromePoliciesKeyPath},
    {HKEY_CURRENT_USER, kChromePoliciesKeyPath}};
const wchar_t kExtensionSettingsRegistryEntryName[] = L"ExtensionSettings";
const char kExtensionSettingsInstallationModeName[] = "installation_mode";
const char kExtensionSettingsForceInstalledValue[] = "force_installed";

const wchar_t kExternalExtensionsFilePath[] =
    L"default_apps\\external_extensions.json";
// Extension IDs that are expected to be in external_extensions.json, or have
// been in past versions, gathered from
// chrome/browser/resources/default_apps/external_extensions.json
constexpr std::array<const base::char16*, 8> default_extension_whitelist = {
    L"blpcfgokakmgnkcojhhkbfbldkacnbeo", L"pjkljhegncpnkpknbcohdijeoejaedia",
    L"apdfllckaahabafndbhieahigkjlhalf", L"aohghmighlieiainnegkcijnfilokake",
    L"aapocclcgogkmnckokdopfmhonfmgoek", L"felcaaldnbdncclmgdcncolpebgiejap",
    L"ghbmnnjooekpmoecnnnilnnbdlolhkhi", L"coobgpohoikkiipiblmjeljniedjpjpf"};

const wchar_t kMasterPreferencesFileName[] = L"master_preferences";

// Removes the extension from the JSON. If the extension is not associated
// wih a valid file and JSON value then this function returns false and
// |json_result| is not modified.
bool RemoveExtensionFromJson(const ForceInstalledExtension& extension,
                             base::Value* json_result) {
  DCHECK(json_result);
  DCHECK(extension.policy_file);
  if (!extension.policy_file->json) {
    return false;
  }

  bool result = RemoveKeyFromDictionary(json_result, extension.id.AsString());
  return result;
}

void GetForcelistPoliciesForAccessMask(
    REGSAM access_mask,
    std::vector<ExtensionPolicyRegistryEntry>* policies) {
  for (size_t i = 0; i < base::size(extension_forcelist_keys); ++i) {
    base::win::RegistryValueIterator forcelist_it(
        extension_forcelist_keys[i].hkey, extension_forcelist_keys[i].path,
        access_mask);
    for (; forcelist_it.Valid(); ++forcelist_it) {
      base::string16 entry;
      GetRegistryValueAsString(forcelist_it.Value(), forcelist_it.ValueSize(),
                               forcelist_it.Type(), &entry);

      // Extract the extension ID from the beginning of the registry entry,
      // since it also contains an update URL.
      if (entry.length() >= kExtensionIdLength) {
        base::string16 extension_id = entry.substr(0, kExtensionIdLength);

        policies->emplace_back(extension_id, extension_forcelist_keys[i].hkey,
                               extension_forcelist_keys[i].path,
                               forcelist_it.Name(), forcelist_it.Type(),
                               nullptr);
      }
    }
  }
}

bool RemoveForcelistPolicyExtensionForAccessMask(
    REGSAM access_mask,
    const ForceInstalledExtension& extension) {
  for (size_t i = 0; i < base::size(extension_forcelist_keys); ++i) {
    std::vector<base::string16> keys;
    base::win::RegistryValueIterator forcelist_it(
        extension_forcelist_keys[i].hkey, extension_forcelist_keys[i].path,
        access_mask);
    for (; forcelist_it.Valid(); ++forcelist_it) {
      base::string16 entry;
      GetRegistryValueAsString(forcelist_it.Value(), forcelist_it.ValueSize(),
                               forcelist_it.Type(), &entry);
      if (base::UTF16ToUTF8(entry.substr(0, kExtensionIdLength)) ==
          extension.id.AsString()) {
        keys.push_back(forcelist_it.Name());
      }
    }
    base::win::RegKey key;
    key.Open(extension_forcelist_keys[i].hkey, extension_forcelist_keys[i].path,
             access_mask | KEY_WRITE);
    for (base::string16& key_name : keys) {
      LONG result = key.DeleteValue(key_name.c_str());
      if (result != ERROR_SUCCESS) {
        LOG(WARNING) << "Could not delete value at key " << key_name
                     << ", error code: " << result;
        return false;
      }
    }
  }
  return true;
}

void GetExtensionSettingsPoliciesFromParsedJson(
    const RegistryKey& registry_key,
    std::vector<ExtensionPolicyRegistryEntry>* policies,
    scoped_refptr<ParseTasksRemainingCounter> counter,
    ContentType type,
    base::Optional<base::Value> json,
    const base::Optional<std::string>& error) {
  base::ScopedClosureRunner closure(
      base::BindOnce(&ParseTasksRemainingCounter::Decrement, counter.get()));

  base::DictionaryValue* extension_settings = nullptr;
  if (!json.has_value() || !json->is_dict() ||
      !json->GetAsDictionary(&extension_settings)) {
    LOG(ERROR) << "Could not read JSON from " << registry_key.hkey << "\\"
               << registry_key.path;
    if (error.has_value()) {
      LOG(ERROR) << "JSON parser error " << error.value();
    }
    return;
  }

  scoped_refptr<RefValue> saved_json =
      base::WrapRefCounted(new RefValue(json->Clone()));
  for (const auto& entry : *extension_settings) {
    const base::string16& extension_id = base::UTF8ToUTF16(entry.first);
    const std::unique_ptr<base::Value>& settings_value = entry.second;

    if (settings_value->is_dict()) {
      base::Value* installation_mode =
          settings_value->FindKey(kExtensionSettingsInstallationModeName);
      if (installation_mode != nullptr &&
          installation_mode->GetString() ==
              kExtensionSettingsForceInstalledValue) {
        policies->emplace_back(
            extension_id, registry_key.hkey, registry_key.path,
            kExtensionSettingsRegistryEntryName, type, saved_json);
      }
    }
  }
}

void GetExtensionSettingsPoliciesForAccessMask(
    REGSAM access_mask,
    JsonParserAPI* json_parser,
    std::vector<ExtensionPolicyRegistryEntry>* policies,
    scoped_refptr<ParseTasksRemainingCounter> counter) {
  for (size_t i = 0; i < base::size(extension_settings_keys); ++i) {
    RegKeyPath key(extension_settings_keys[i].hkey,
                   extension_settings_keys[i].path, access_mask);
    base::string16 extension_settings;
    RegistryError error;
    ContentType type;
    ReadRegistryValue(key, kExtensionSettingsRegistryEntryName,
                      &extension_settings, &type, &error);

    if (error != RegistryError::SUCCESS) {
      LOG_IF(WARNING, error != RegistryError::VALUE_NOT_FOUND)
          << "Failed to read string registry value: '"
          << extension_settings_keys[i].path << "\\"
          << kExtensionSettingsRegistryEntryName << "', error: '"
          << static_cast<int>(error) << "'.";
      continue;
    }

    counter->Increment();
    json_parser->Parse(
        base::UTF16ToUTF8(extension_settings),
        base::BindOnce(&GetExtensionSettingsPoliciesFromParsedJson,
                       extension_settings_keys[i], policies, counter, type));
  }
}

void GetDefaultExtensionsFromParsedJson(
    const base::FilePath& extensions_file,
    std::vector<ExtensionPolicyFile>* policies,
    scoped_refptr<ParseTasksRemainingCounter> counter,
    base::Optional<base::Value> json,
    const base::Optional<std::string>& error) {
  base::ScopedClosureRunner closure(
      base::BindOnce(&ParseTasksRemainingCounter::Decrement, counter.get()));

  base::DictionaryValue* default_extensions = nullptr;
  if (!json.has_value() || !json->is_dict() ||
      !json->GetAsDictionary(&default_extensions)) {
    LOG(ERROR) << "Could not read JSON from " << SanitizePath(extensions_file);
    if (error.has_value()) {
      LOG(ERROR) << "JSON parser error " << error.value();
    }
    return;
  }

  scoped_refptr<RefValue> saved_json =
      base::WrapRefCounted(new RefValue(json->Clone()));
  for (const auto& entry : *default_extensions) {
    base::string16 extension_id = base::UTF8ToUTF16(entry.first);
    if (!base::Contains(default_extension_whitelist, extension_id)) {
      policies->emplace_back(extension_id, extensions_file, saved_json);
    }
  }
}

void GetMasterPreferencesExtensionsFromParsedJson(
    const base::FilePath& extensions_file,
    std::vector<ExtensionPolicyFile>* policies,
    scoped_refptr<ParseTasksRemainingCounter> counter,
    base::Optional<base::Value> json,
    const base::Optional<std::string>& error) {
  base::ScopedClosureRunner closure(
      base::BindOnce(&ParseTasksRemainingCounter::Decrement, counter.get()));

  base::DictionaryValue* master_preferences = nullptr;
  if (!json.has_value() || !json->is_dict() ||
      !json->GetAsDictionary(&master_preferences)) {
    LOG(ERROR) << "Could not read JSON from " << SanitizePath(extensions_file);
    if (error.has_value()) {
      LOG(ERROR) << "JSON parser error " << error.value();
    }
    return;
  }

  base::Value* extension_settings = master_preferences->FindPathOfType(
      {"extensions", "settings"}, base::Value::Type::DICTIONARY);
  if (extension_settings == nullptr)
    return;

  base::DictionaryValue* extension_settings_dictionary;
  extension_settings->GetAsDictionary(&extension_settings_dictionary);
  scoped_refptr<RefValue> saved_json =
      base::WrapRefCounted(new RefValue(json->Clone()));
  for (const auto& entry : *extension_settings_dictionary) {
    base::string16 extension_id = base::UTF8ToUTF16(entry.first);
    policies->emplace_back(extension_id, extensions_file, saved_json);
  }
}

}  // namespace

ExtensionPolicyRegistryEntry::ExtensionPolicyRegistryEntry(
    const base::string16& extension_id,
    HKEY hkey,
    const base::string16& path,
    const base::string16& name,
    ContentType content_type,
    scoped_refptr<RefValue> json)
    : extension_id(extension_id),
      hkey(hkey),
      path(path),
      name(name),
      content_type(content_type),
      json(std::move(json)) {}

ExtensionPolicyRegistryEntry::ExtensionPolicyRegistryEntry(
    ExtensionPolicyRegistryEntry&&) = default;

ExtensionPolicyRegistryEntry::~ExtensionPolicyRegistryEntry() = default;

ExtensionPolicyRegistryEntry& ExtensionPolicyRegistryEntry::operator=(
    ExtensionPolicyRegistryEntry&&) = default;

ExtensionPolicyFile::ExtensionPolicyFile(const base::string16& extension_id,
                                         const base::FilePath& path,
                                         scoped_refptr<RefValue> json)
    : extension_id(extension_id), path(path), json(std::move(json)) {}

ExtensionPolicyFile::ExtensionPolicyFile(ExtensionPolicyFile&&) = default;

ExtensionPolicyFile::~ExtensionPolicyFile() = default;

ExtensionPolicyFile& ExtensionPolicyFile::operator=(ExtensionPolicyFile&&) =
    default;

void GetExtensionForcelistRegistryPolicies(
    std::vector<ExtensionPolicyRegistryEntry>* policies) {
  GetForcelistPoliciesForAccessMask(KEY_WOW64_32KEY, policies);
  if (IsX64Architecture())
    GetForcelistPoliciesForAccessMask(KEY_WOW64_64KEY, policies);
}

bool RemoveForcelistPolicyExtension(const ForceInstalledExtension& extension) {
  DCHECK(extension.install_method == POLICY_EXTENSION_FORCELIST);
  // No need to check for policy_registry_entry, as it's not used in deletion.

  bool result =
      RemoveForcelistPolicyExtensionForAccessMask(KEY_WOW64_32KEY, extension);
  if (IsX64Architecture() && result) {
    result =
        RemoveForcelistPolicyExtensionForAccessMask(KEY_WOW64_64KEY, extension);
  }
  return result;
}

void GetNonWhitelistedDefaultExtensions(
    JsonParserAPI* json_parser,
    std::vector<ExtensionPolicyFile>* policies,
    base::WaitableEvent* done) {
  std::set<base::FilePath> install_paths;
  ListChromeInstallationPaths(&install_paths);

  std::map<base::FilePath, std::string> files_read;
  for (const base::FilePath& folder : install_paths) {
    const base::FilePath& extensions_file(
        folder.Append(kExternalExtensionsFilePath));
    if (!base::PathExists(extensions_file))
      continue;

    // Read the external_extensions JSON file.
    std::string content;
    if (!base::ReadFileToString(extensions_file, &content)) {
      PLOG(ERROR) << "Failed to read file: " << SanitizePath(extensions_file);
      continue;
    }
    files_read[extensions_file] = content;
  }

  if (files_read.size() == 0) {
    done->Signal();
    return;
  }

  scoped_refptr<ParseTasksRemainingCounter> counter =
      base::MakeRefCounted<ParseTasksRemainingCounter>(files_read.size(), done);
  for (const auto& file : files_read) {
    const base::FilePath& path = file.first;
    const std::string& content = file.second;
    json_parser->Parse(content,
                       base::BindOnce(&GetDefaultExtensionsFromParsedJson, path,
                                      policies, counter));
  }
}

bool RemoveDefaultExtension(const ForceInstalledExtension& extension,
                            base::Value* json_result) {
  DCHECK(extension.install_method == DEFAULT_APPS_EXTENSION);
  return RemoveExtensionFromJson(extension, json_result);
}

void GetExtensionSettingsForceInstalledExtensions(
    JsonParserAPI* json_parser,
    std::vector<ExtensionPolicyRegistryEntry>* policies,
    base::WaitableEvent* done) {
  // Make a counter with initial count 1 so it doesn't potentially signal until
  // after all parse tasks are posted.
  scoped_refptr<ParseTasksRemainingCounter> counter =
      base::MakeRefCounted<ParseTasksRemainingCounter>(1, done);
  GetExtensionSettingsPoliciesForAccessMask(KEY_WOW64_32KEY, json_parser,
                                            policies, counter);
  if (IsX64Architecture()) {
    GetExtensionSettingsPoliciesForAccessMask(KEY_WOW64_64KEY, json_parser,
                                              policies, counter);
  }
  // Decrement so that the counter can signal when it hits 0.
  counter->Decrement();
}

bool RemoveExtensionSettingsPoliciesExtension(
    const ForceInstalledExtension& extension,
    base::Value* json_result) {
  DCHECK(extension.install_method == POLICY_EXTENSION_SETTINGS);
  DCHECK(extension.policy_registry_entry);
  DCHECK(json_result);

  if (!extension.policy_registry_entry->json.get()) {
    return false;
  }
  return RemoveKeyFromDictionary(json_result, extension.id.AsString());
}

void GetMasterPreferencesExtensions(JsonParserAPI* json_parser,
                                    std::vector<ExtensionPolicyFile>* policies,
                                    base::WaitableEvent* done) {
  std::set<base::FilePath> exe_paths;
  ListChromeExePaths(&exe_paths);

  std::map<base::FilePath, std::string> files_read;
  for (const base::FilePath& path : exe_paths) {
    const base::FilePath& master_preferences(
        path.Append(kMasterPreferencesFileName));
    if (!base::PathExists(master_preferences))
      continue;

    std::string content;
    if (!base::ReadFileToString(master_preferences, &content)) {
      PLOG(ERROR) << "Failed to read file: "
                  << SanitizePath(master_preferences);
      continue;
    }
    files_read[master_preferences] = content;
  }

  if (files_read.size() == 0) {
    done->Signal();
    return;
  }

  scoped_refptr<ParseTasksRemainingCounter> counter =
      base::MakeRefCounted<ParseTasksRemainingCounter>(files_read.size(), done);
  for (const auto& file : files_read) {
    const base::FilePath& path = file.first;
    const std::string& content = file.second;
    json_parser->Parse(
        content, base::BindOnce(&GetMasterPreferencesExtensionsFromParsedJson,
                                path, policies, counter));
  }
}

bool RemoveMasterPreferencesExtension(const ForceInstalledExtension& extension,
                                      base::Value* json_result) {
  DCHECK(extension.install_method == POLICY_MASTER_PREFERENCES);
  DCHECK(json_result);
  DCHECK(json_result->is_dict());
  // The extensions are stored in ["extensions"]["settings"]
  base::Value* sub_dictionary =
      json_result->FindPath({"extensions", "settings"});
  return RemoveExtensionFromJson(extension, sub_dictionary);
}

}  // namespace chrome_cleaner
