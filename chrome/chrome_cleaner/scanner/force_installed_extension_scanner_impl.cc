// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/force_installed_extension_scanner_impl.h"

#include <memory>
#include <set>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/chrome_cleaner/chrome_utils/extensions_util.h"
#include "chrome/chrome_cleaner/chrome_utils/force_installed_extension.h"
#include "chrome/chrome_cleaner/os/resource_util.h"

namespace chrome_cleaner {

using base::WaitableEvent;

ForceInstalledExtensionScannerImpl::ForceInstalledExtensionScannerImpl() =
    default;
ForceInstalledExtensionScannerImpl::~ForceInstalledExtensionScannerImpl() =
    default;

std::unique_ptr<UwEMatchers>
ForceInstalledExtensionScannerImpl::CreateUwEMatchersFromResource(
    int resource_id) {
  if (!resource_id) {
    // Use empty matchers.
    LOG(WARNING) << "No UwE matchers set";
    return std::make_unique<chrome_cleaner::UwEMatchers>();
  }
  base::StringPiece serialized_matcher_pb;
  if (!chrome_cleaner::LoadResourceOfKind(resource_id, L"TEXT",
                                          &serialized_matcher_pb)) {
    LOG(DFATAL) << "Failed to load expected UwE matchers from resource id "
                << resource_id;
    return nullptr;
  }
  auto uwe_matchers = std::make_unique<chrome_cleaner::UwEMatchers>();
  uwe_matchers->ParseFromString(serialized_matcher_pb.as_string());
  return uwe_matchers;
}

std::vector<ForceInstalledExtension>
ForceInstalledExtensionScannerImpl::GetForceInstalledExtensions(
    JsonParserAPI* json_parser) {
  const base::TimeTicks end_time =
      base::TimeTicks::Now() +
      base::TimeDelta::FromMilliseconds(kParseAttemptTimeoutMilliseconds);

  std::vector<ExtensionPolicyRegistryEntry> policy_registry_entries_force_list;
  GetExtensionForcelistRegistryPolicies(&policy_registry_entries_force_list);

  std::vector<ExtensionPolicyFile> policy_files_default_extensions;
  WaitableEvent non_whitelist_default_extensions_done{
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED};
  GetNonWhitelistedDefaultExtensions(json_parser,
                                     &policy_files_default_extensions,
                                     &non_whitelist_default_extensions_done);

  std::vector<ExtensionPolicyRegistryEntry>
      policy_registry_entries_force_installed;
  WaitableEvent settings_force_installed_done{
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED};
  GetExtensionSettingsForceInstalledExtensions(
      json_parser, &policy_registry_entries_force_installed,
      &settings_force_installed_done);

  std::vector<ExtensionPolicyFile> policy_files_master_preferences;
  WaitableEvent master_preferences_done{
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED};
  GetMasterPreferencesExtensions(json_parser, &policy_files_master_preferences,
                                 &master_preferences_done);

  std::set<ForceInstalledExtension> result;
  while (policy_registry_entries_force_list.size() > 0) {
    ExtensionPolicyRegistryEntry entry =
        std::move(policy_registry_entries_force_list.back());
    policy_registry_entries_force_list.pop_back();
    base::Optional<ExtensionID> extension_id =
        ExtensionID::Create(base::UTF16ToUTF8(entry.extension_id));
    if (!extension_id.has_value()) {
      continue;
    }
    ForceInstalledExtension extension(extension_id.value(),
                                      POLICY_EXTENSION_FORCELIST);
    result.emplace(std::move(extension));
  }

  non_whitelist_default_extensions_done.TimedWait(end_time -
                                                  base::TimeTicks::Now());
  while (policy_files_default_extensions.size() > 0) {
    ExtensionPolicyFile file =
        std::move(policy_files_default_extensions.back());
    policy_files_default_extensions.pop_back();
    base::Optional<ExtensionID> extension_id =
        ExtensionID::Create(base::UTF16ToUTF8(file.extension_id));
    if (!extension_id.has_value()) {
      continue;
    }
    ForceInstalledExtension extension(extension_id.value(),
                                      DEFAULT_APPS_EXTENSION);
    extension.policy_file =
        std::make_shared<ExtensionPolicyFile>(std::move(file));
    result.emplace(std::move(extension));
  }

  settings_force_installed_done.TimedWait(end_time - base::TimeTicks::Now());
  while (policy_registry_entries_force_installed.size() > 0) {
    ExtensionPolicyRegistryEntry entry =
        std::move(policy_registry_entries_force_installed.back());
    policy_registry_entries_force_installed.pop_back();
    base::Optional<ExtensionID> extension_id =
        ExtensionID::Create(base::UTF16ToUTF8(entry.extension_id));
    if (!extension_id.has_value()) {
      continue;
    }
    ForceInstalledExtension extension(extension_id.value(),
                                      POLICY_EXTENSION_SETTINGS);
    extension.policy_registry_entry =
        std::make_shared<ExtensionPolicyRegistryEntry>(std::move(entry));
    result.emplace(std::move(extension));
  }

  master_preferences_done.TimedWait(end_time - base::TimeTicks::Now());
  while (policy_files_master_preferences.size() > 0) {
    ExtensionPolicyFile file =
        std::move(policy_files_master_preferences.back());
    policy_files_master_preferences.pop_back();
    base::Optional<ExtensionID> extension_id =
        ExtensionID::Create(base::UTF16ToUTF8(file.extension_id));
    if (!extension_id.has_value()) {
      continue;
    }
    ForceInstalledExtension extension(extension_id.value(),
                                      POLICY_MASTER_PREFERENCES);
    extension.policy_file =
        std::make_shared<ExtensionPolicyFile>(std::move(file));
    result.emplace(std::move(extension));
  }

  return std::vector<ForceInstalledExtension>(result.begin(), result.end());
}

}  // namespace chrome_cleaner
