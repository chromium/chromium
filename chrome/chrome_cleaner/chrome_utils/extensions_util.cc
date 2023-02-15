// Copyright 2018 The Chromium Authors
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

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/chrome_utils/chrome_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/registry.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/parsers/parser_utils/parse_tasks_remaining_counter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// TODO(crbug.com/981388): See if there's anything that isn't used from
// system_report_component.cc that can be removed from this file.

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
constexpr std::array<const wchar_t*, 8> default_extension_whitelist = {
    L"blpcfgokakmgnkcojhhkbfbldkacnbeo", L"pjkljhegncpnkpknbcohdijeoejaedia",
    L"apdfllckaahabafndbhieahigkjlhalf", L"aohghmighlieiainnegkcijnfilokake",
    L"aapocclcgogkmnckokdopfmhonfmgoek", L"felcaaldnbdncclmgdcncolpebgiejap",
    L"ghbmnnjooekpmoecnnnilnnbdlolhkhi", L"coobgpohoikkiipiblmjeljniedjpjpf"};

const wchar_t kMasterPreferencesFileName[] = L"master_preferences";

void GetForcelistPoliciesForAccessMask(
    REGSAM access_mask,
    std::vector<ExtensionPolicyRegistryEntry>* policies) {
  for (size_t i = 0; i < std::size(extension_forcelist_keys); ++i) {
    base::win::RegistryValueIterator forcelist_it(
        extension_forcelist_keys[i].hkey, extension_forcelist_keys[i].path,
        access_mask);
    for (; forcelist_it.Valid(); ++forcelist_it) {
      std::wstring entry;
      GetRegistryValueAsString(forcelist_it.Value(), forcelist_it.ValueSize(),
                               forcelist_it.Type(), &entry);

      // Extract the extension ID from the beginning of the registry entry,
      // since it also contains an update URL.
      if (entry.length() >= kExtensionIdLength) {
        std::wstring extension_id = entry.substr(0, kExtensionIdLength);

        policies->emplace_back(extension_id, extension_forcelist_keys[i].hkey,
                               extension_forcelist_keys[i].path,
                               forcelist_it.Name(), forcelist_it.Type(),
                               nullptr);
      }
    }
  }
}

void GetExtensionSettingsPoliciesFromParsedJson(
    const RegistryKey& registry_key,
    std::vector<ExtensionPolicyRegistryEntry>* policies,
    scoped_refptr<ParseTasksRemainingCounter> counter,
    ContentType type,
    absl::optional<base::Value> json,
    const absl::optional<std::string>& error) {
  base::ScopedClosureRunner closure(
      base::BindOnce(&ParseTasksRemainingCounter::Decrement, counter.get()));

  base::Value::Dict* extension_settings =
      json.has_value() ? json->GetIfDict() : nullptr;
  if (!extension_settings) {
    LOG(ERROR) << "Could not read JSON from " << registry_key.hkey << "\\"
               << registry_key.path;
    if (error.has_value()) {
      LOG(ERROR) << "JSON parser error " << error.value();
    }
    return;
  }

  scoped_refptr<RefValue> saved_json =
      base::WrapRefCounted(new RefValue(json->Clone()));
  for (auto entry : *extension_settings) {
    const std::wstring& extension_id = base::UTF8ToWide(entry.first);
    const base::Value& settings_value = entry.second;

    if (settings_value.is_dict()) {
      const std::string* installation_mode =
          settings_value.GetDict().FindString(
              kExtensionSettingsInstallationModeName);
      if (installation_mode &&
          *installation_mode == kExtensionSettingsForceInstalledValue) {
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
  for (size_t i = 0; i < std::size(extension_settings_keys); ++i) {
    RegKeyPath key(extension_settings_keys[i].hkey,
                   extension_settings_keys[i].path, access_mask);
    std::wstring extension_settings;
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
        base::WideToUTF8(extension_settings),
        base::BindOnce(&GetExtensionSettingsPoliciesFromParsedJson,
                       extension_settings_keys[i], policies, counter, type));
  }
}

void GetDefaultExtensionsFromParsedJson(
    const base::FilePath& extensions_file,
    std::vector<ExtensionPolicyFile>* policies,
    scoped_refptr<ParseTasksRemainingCounter> counter,
    absl::optional<base::Value> json,
    const absl::optional<std::string>& error) {
  base::ScopedClosureRunner closure(
      base::BindOnce(&ParseTasksRemainingCounter::Decrement, counter.get()));

  base::Value::Dict* default_extensions =
      json.has_value() ? json->GetIfDict() : nullptr;
  if (!default_extensions) {
    LOG(ERROR) << "Could not read JSON from " << SanitizePath(extensions_file);
    if (error.has_value()) {
      LOG(ERROR) << "JSON parser error " << error.value();
    }
    return;
  }

  scoped_refptr<RefValue> saved_json =
      base::WrapRefCounted(new RefValue(json->Clone()));
  for (auto entry : *default_extensions) {
    std::wstring extension_id = base::UTF8ToWide(entry.first);
    if (!base::Contains(default_extension_whitelist, extension_id)) {
      policies->emplace_back(extension_id, extensions_file, saved_json);
    }
  }
}

void GetMasterPreferencesExtensionsFromParsedJson(
    const base::FilePath& extensions_file,
    std::vector<ExtensionPolicyFile>* policies,
    scoped_refptr<ParseTasksRemainingCounter> counter,
    absl::optional<base::Value> json,
    const absl::optional<std::string>& error) {
  base::ScopedClosureRunner closure(
      base::BindOnce(&ParseTasksRemainingCounter::Decrement, counter.get()));

  base::Value::Dict* master_preferences =
      json.has_value() ? json->GetIfDict() : nullptr;
  if (!master_preferences) {
    LOG(ERROR) << "Could not read JSON from " << SanitizePath(extensions_file);
    if (error.has_value()) {
      LOG(ERROR) << "JSON parser error " << error.value();
    }
    return;
  }

  base::Value::Dict* extension_settings_dictionary =
      master_preferences->FindDictByDottedPath("extensions.settings");
  if (!extension_settings_dictionary)
    return;

  scoped_refptr<RefValue> saved_json =
      base::WrapRefCounted(new RefValue(json->Clone()));
  for (auto entry : *extension_settings_dictionary) {
    std::wstring extension_id = base::UTF8ToWide(entry.first);
    policies->emplace_back(extension_id, extensions_file, saved_json);
  }
}

}  // namespace

ExtensionPolicyRegistryEntry::ExtensionPolicyRegistryEntry(
    const std::wstring& extension_id,
    HKEY hkey,
    const std::wstring& path,
    const std::wstring& name,
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

ExtensionPolicyFile::ExtensionPolicyFile(const std::wstring& extension_id,
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

void GetMasterPreferencesExtensions(JsonParserAPI* json_parser,
                                    std::vector<ExtensionPolicyFile>* policies,
                                    base::WaitableEvent* done) {
  std::set<base::FilePath> chrome_exe_directories;
  ListChromeExeDirectories(&chrome_exe_directories);

  std::map<base::FilePath, std::string> files_read;
  for (const base::FilePath& path : chrome_exe_directories) {
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

}  // namespace chrome_cleaner
