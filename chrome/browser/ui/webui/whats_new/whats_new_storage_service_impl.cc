// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_storage_service_impl.h"

#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"

namespace whats_new {
const base::Value::List& WhatsNewStorageServiceImpl::ReadModuleData() const {
  const base::Value::List& editions = g_browser_process->local_state()->GetList(
      prefs::kWhatsNewFirstEnabledOrder);
  return editions;
}

const base::Value::Dict& WhatsNewStorageServiceImpl::ReadEditionData() const {
  const base::Value::Dict& editions =
      g_browser_process->local_state()->GetDict(prefs::kWhatsNewEditionUsed);
  return editions;
}

std::optional<int> WhatsNewStorageServiceImpl::ReadVersionData() const {
  return g_browser_process->local_state()->GetInteger(
      prefs::kWhatsNewVersionUsed);
}

int WhatsNewStorageServiceImpl::GetModuleQueuePosition(
    std::string_view module_name) const {
  const base::Value::List& module_data = ReadModuleData();
  auto order = std::find_if(module_data.begin(), module_data.end(),
                            [&](auto& ordered_module_name) {
                              return ordered_module_name == module_name;
                            });
  return order == module_data.end() ? -1 : order - module_data.begin();
}

std::optional<int> WhatsNewStorageServiceImpl::GetUsedVersion(
    std::string_view edition_name) const {
  const base::Value* version = ReadEditionData().Find(edition_name);
  return version == nullptr ? std::nullopt : std::optional(version->GetInt());
}

std::optional<std::string_view>
WhatsNewStorageServiceImpl::FindEditionForCurrentVersion() const {
  const base::Value::Dict& edition_data = ReadEditionData();
  auto edition_for_version = std::find_if(
      edition_data.begin(), edition_data.end(),
      [](auto edition) { return edition.second == CHROME_VERSION_MAJOR; });
  return edition_for_version == edition_data.end()
             ? std::nullopt
             : std::optional(edition_for_version->first.c_str());
}

void WhatsNewStorageServiceImpl::SetModuleEnabled(
    std::string_view module_name) {
  // Ensure active feature is in local state.
  const base::Value::List& enabled_modules = ReadModuleData();
  if (!base::Contains(enabled_modules, module_name)) {
    GetEnabledOrder()->Append(module_name);
  }
}

bool WhatsNewStorageServiceImpl::IsUsedEdition(
    std::string_view edition_name) const {
  return GetUsedVersion(edition_name) != std::nullopt;
}

bool WhatsNewStorageServiceImpl::WasVersionPageUsedForCurrentMilestone() const {
  return ReadVersionData() == CHROME_VERSION_MAJOR;
}

void WhatsNewStorageServiceImpl::SetEditionUsed(std::string_view edition_name) {
  // Edition should not be previously used.
  auto stored_version = GetUsedVersion(edition_name);
  if (stored_version.has_value()) {
    return;
  }

  // No other edition should be marked as used for this version.
  const base::Value::Dict& edition_data = ReadEditionData();
  auto edition_for_version = std::find_if(
      edition_data.begin(), edition_data.end(),
      [](auto edition) { return edition.second == CHROME_VERSION_MAJOR; });
  if (edition_for_version != edition_data.end()) {
    return;
  }

  GetUsedEditions()->Set(edition_name, CHROME_VERSION_MAJOR);
}

void WhatsNewStorageServiceImpl::SetVersionUsed() {
  g_browser_process->local_state()->SetInteger(prefs::kWhatsNewVersionUsed,
                                               CHROME_VERSION_MAJOR);
}

void WhatsNewStorageServiceImpl::ClearModules(
    std::set<std::string_view> modules_to_clear) {
  // Remove rolled feature from prefs. Order no longer matters for
  // rolled modules.
  auto enabled_modules = GetEnabledOrder();
  for (const auto module : modules_to_clear) {
    enabled_modules->EraseValue(base::Value(module));
  }
}

void WhatsNewStorageServiceImpl::ClearEditions(
    std::set<std::string_view> editions_to_clear) {
  // Remove edition from prefs.
  auto used_editions = GetUsedEditions();
  for (const auto edition : editions_to_clear) {
    used_editions->Remove(edition);
  }
}

void WhatsNewStorageServiceImpl::Reset() {
  GetEnabledOrder()->clear();
  GetUsedEditions()->clear();
}

ScopedListPrefUpdate WhatsNewStorageServiceImpl::GetEnabledOrder() {
  return ScopedListPrefUpdate(g_browser_process->local_state(),
                              prefs::kWhatsNewFirstEnabledOrder);
}

ScopedDictPrefUpdate WhatsNewStorageServiceImpl::GetUsedEditions() {
  return ScopedDictPrefUpdate(g_browser_process->local_state(),
                              prefs::kWhatsNewEditionUsed);
}

WhatsNewStorageServiceImpl::~WhatsNewStorageServiceImpl() = default;
}  // namespace whats_new
