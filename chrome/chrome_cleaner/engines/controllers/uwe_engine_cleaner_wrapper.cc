// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/uwe_engine_cleaner_wrapper.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/chrome_utils/extensions_util.h"
#include "chrome/chrome_cleaner/chrome_utils/force_installed_extension.h"
#include "chrome/chrome_cleaner/os/system_util.h"

namespace chrome_cleaner {

using base::ImportantFileWriter;
using ExtensionFilePath = base::string16;
using ExtensionRegistryPath = base::string16;

namespace {

// TODO(joenotcharles): Use RegKeyPath instead.
struct RegistryKey {
  HKEY hkey;
  const wchar_t* path;
};

const wchar_t kExtensionSettingsRegistryEntryName[] = L"ExtensionSettings";
const RegistryKey extension_forcelist_keys[] = {
    {HKEY_LOCAL_MACHINE, kChromePoliciesKeyPath},
    {HKEY_CURRENT_USER, kChromePoliciesKeyPath}};

bool RemoveExtensionSettingsPoliciesExtensionForAccessMask(
    REGSAM access_mask,
    ContentType content_type,
    const base::string16& serialized_json) {
  for (size_t i = 0; i < base::size(extension_forcelist_keys); ++i) {
    base::win::RegKey key(extension_forcelist_keys[i].hkey,
                          extension_forcelist_keys[i].path, access_mask);
    if (!WriteRegistryValue(kExtensionSettingsRegistryEntryName,
                            serialized_json, content_type, &key)) {
      return false;
    }
  }
  return true;
}

}  // namespace

UwEEngineCleanerWrapper::UwEEngineCleanerWrapper(
    std::unique_ptr<Cleaner> cleaner,
    DisableExtensionsCallback disable_extensions_callback,
    ChromePromptIPC* chrome_prompt_ipc)
    : cleaner_(std::move(cleaner)),
      disable_extensions_callback_(std::move(disable_extensions_callback)) {
  chrome_prompt_ipc_ = chrome_prompt_ipc;
  DCHECK(cleaner_);
}

UwEEngineCleanerWrapper::~UwEEngineCleanerWrapper() = default;

void UwEEngineCleanerWrapper::RemovePUPExtensions(
    const std::vector<UwSId>& pup_ids) {
  extension_removal_result_ = RESULT_CODE_FAILED;
  base::ScopedClosureRunner done_closure(task_barrier_closure_);
  // The modified JSON contents that we build up until we write out the contents
  // at the end.
  std::map<ExtensionFilePath, base::Value> default_extension_json;
  std::map<ExtensionRegistryPath, std::pair<base::Value, ContentType>>
      extension_settings_policies_json;
  std::map<ExtensionFilePath, base::Value> master_preferences_json;
  // Filter out any duplicates by extension id so we don't remove them twice.
  std::set<ForceInstalledExtension, ExtensionIDCompare> extensions;
  for (const UwSId& pup_id : pup_ids) {
    PUPData::PUP* pup = PUPData::GetPUP(pup_id);
    DCHECK(pup);
    extensions.insert(pup->matched_extensions.begin(),
                      pup->matched_extensions.end());
  }
  std::vector<base::string16> master_preferences_extensions;
  for (const ForceInstalledExtension& extension : extensions) {
    switch (extension.install_method) {
      case DEFAULT_APPS_EXTENSION: {
        auto entry =
            default_extension_json.find(extension.policy_file->path.value());
        if (entry == default_extension_json.end()) {
          base::Value new_json = extension.policy_file->json->data.Clone();
          entry = default_extension_json
                      .insert({extension.policy_file->path.value(),
                               std::move(new_json)})
                      .first;
        }
        if (!RemoveDefaultExtension(extension, &entry->second)) {
          LOG(ERROR) << "Could not remove default extension "
                     << extension.id.AsString();
          return;
        }
        break;
      }
      case POLICY_EXTENSION_FORCELIST: {
        if (!RemoveForcelistPolicyExtension(extension)) {
          LOG(ERROR) << "Could not remove Forcelist Policy extension "
                     << extension.id.AsString();
          return;
        }
        break;
      }
      case POLICY_EXTENSION_SETTINGS: {
        auto entry = extension_settings_policies_json.find(
            extension.policy_registry_entry->path);
        if (entry == extension_settings_policies_json.end()) {
          DCHECK(extension.policy_registry_entry);
          DCHECK(extension.policy_registry_entry->json);
          base::Value new_json =
              extension.policy_registry_entry->json->data.Clone();
          std::pair<base::Value, ContentType> entry_value =
              std::make_pair(std::move(new_json),
                             extension.policy_registry_entry->content_type);
          entry = extension_settings_policies_json
                      .insert({extension.policy_registry_entry->path,
                               std::move(entry_value)})
                      .first;
        }
        base::Value* json_entry = &entry->second.first;
        if (!RemoveExtensionSettingsPoliciesExtension(extension, json_entry)) {
          LOG(ERROR) << "Could not remove Policy Settings extension "
                     << extension.id.AsString();
          return;
        }
        break;
      }
      case POLICY_MASTER_PREFERENCES: {
        auto entry =
            master_preferences_json.find(extension.policy_file->path.value());
        if (entry == master_preferences_json.end()) {
          base::Value new_json = extension.policy_file->json->data.Clone();
          entry = master_preferences_json
                      .insert({extension.policy_file->path.value(),
                               std::move(new_json)})
                      .first;
        }
        if (!RemoveMasterPreferencesExtension(extension, &entry->second)) {
          LOG(ERROR) << "Could not remove master preferences extension "
                     << extension.id.AsString();
          return;
        }
        master_preferences_extensions.push_back(
            base::UTF8ToUTF16(extension.id.AsString()));
        break;
      }
      case INSTALL_METHOD_UNSPECIFIED:
      default: {
        LOG(ERROR) << "Unknown extension force install method "
                   << extension.install_method;
        return;
      }
    }
  }
  for (const auto& entry : default_extension_json) {
    std::string serialized_json;
    JSONStringValueSerializer serializer(&serialized_json);
    if (!serializer.Serialize(entry.second)) {
      LOG(ERROR) << "Could not serialize json";
      return;
    }
    if (!ImportantFileWriter::WriteFileAtomically(base::FilePath(entry.first),
                                                  serialized_json, "")) {
      LOG(ERROR) << "Could not write default extensions json to file "
                 << SanitizePath(base::FilePath(entry.first));
      return;
    }
  }
  for (const auto& entry : extension_settings_policies_json) {
    std::string serialized_json;
    const base::Value& json = entry.second.first;
    ContentType content_type = entry.second.second;
    JSONStringValueSerializer serializer(&serialized_json);
    if (!serializer.Serialize(json)) {
      LOG(ERROR) << "Could not serialize json";
      return;
    }
    base::string16 serialized_json16 = base::UTF8ToUTF16(serialized_json);
    if (!RemoveExtensionSettingsPoliciesExtensionForAccessMask(
            KEY_WOW64_32KEY | KEY_WRITE, content_type, serialized_json16)) {
      LOG(ERROR) << "Could not remove extension settings from registry";
      return;
    }
    if (IsX64Architecture()) {
      if (!RemoveExtensionSettingsPoliciesExtensionForAccessMask(
              KEY_WOW64_64KEY | KEY_WRITE, content_type, serialized_json16)) {
        LOG(ERROR) << "Could not remove extension settings from registry";
        return;
      }
    }
  }
  for (const auto& entry : master_preferences_json) {
    std::string serialized_json;
    JSONStringValueSerializer serializer(&serialized_json);
    if (!serializer.Serialize(entry.second)) {
      LOG(ERROR) << "Could not serialize json";
      return;
    }
    if (!ImportantFileWriter::WriteFileAtomically(base::FilePath(entry.first),
                                                  serialized_json, "")) {
      LOG(ERROR) << "Could not write master preferences json to file "
                 << SanitizePath(base::FilePath(entry.first));
      return;
    }
  }
  // avoid the round trip if there are no master preferences extensions
  // to remove.
  if (master_preferences_extensions.size() > 0) {
    done_closure.ReplaceClosure(base::DoNothing());
    std::move(disable_extensions_callback_)
        .Run(const_cast<std::vector<base::string16>&>(
                 master_preferences_extensions),
             base::BindOnce(&UwEEngineCleanerWrapper::DisableExtensionDone,
                            base::Unretained(this)));
  } else {
    extension_removal_result_ = RESULT_CODE_SUCCESS;
  }
}

void UwEEngineCleanerWrapper::DisableExtensionDone(bool result) {
  if (result) {
    extension_removal_result_ = RESULT_CODE_SUCCESS;
  }
  task_barrier_closure_.Run();
}

void UwEEngineCleanerWrapper::OnTotallyDone(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  is_totally_done_ = true;
  ResultCode final_result = uws_removal_result_ != RESULT_CODE_SUCCESS
                                ? uws_removal_result_
                                : extension_removal_result_;
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(done_callback_), final_result));
}

void UwEEngineCleanerWrapper::OnDoneUwSCleanup(ResultCode status) {
  uws_removal_result_ = status;
  task_barrier_closure_.Run();
}

void UwEEngineCleanerWrapper::Start(const std::vector<UwSId>& pup_ids,
                                    DoneCallback done_callback) {
  is_totally_done_ = false;
  done_callback_ = std::move(done_callback);
  task_barrier_closure_ = base::BarrierClosure(
      2, base::BindOnce(&UwEEngineCleanerWrapper::OnTotallyDone,
                        base::Unretained(this),
                        base::SequencedTaskRunnerHandle::Get()));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
           base::BindOnce(&UwEEngineCleanerWrapper::TryRemovePUPExtensions,
                          base::Unretained(this), pup_ids));

  cleaner_->Start(pup_ids,
                  base::BindOnce(&UwEEngineCleanerWrapper::OnDoneUwSCleanup,
                                 base::Unretained(this)));
}

void UwEEngineCleanerWrapper::TryRemovePUPExtensions(
    const std::vector<UwSId>& pup_ids) {
  if (chrome_prompt_ipc_) {
    chrome_prompt_ipc_->TryDeleteExtensions(
        base::BindOnce(&UwEEngineCleanerWrapper::RemovePUPExtensions,
                       base::Unretained(this), std::move(pup_ids)),
        base::BindOnce(&UwEEngineCleanerWrapper::DisableExtensionDone,
                       base::Unretained(this), true));
  } else {
    DisableExtensionDone(true);
  }
}

void UwEEngineCleanerWrapper::StartPostReboot(const std::vector<UwSId>& pup_ids,
                                              DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_totally_done_ = true;
  cleaner_->StartPostReboot(pup_ids, std::move(done_callback));
}

void UwEEngineCleanerWrapper::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_totally_done_ = true;
  extension_removal_result_ = RESULT_CODE_FAILED;
  cleaner_->Stop();
}

bool UwEEngineCleanerWrapper::IsCompletelyDone() const {
  return cleaner_->IsCompletelyDone() && is_totally_done_;
}

bool UwEEngineCleanerWrapper::CanClean(const std::vector<UwSId>& pup_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cleaner_->CanClean(pup_ids);
}

}  // namespace chrome_cleaner
