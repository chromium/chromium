// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/override_manifest_asset_manager_delegate.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"

namespace optimization_guide {

OverrideManifestAssetManagerDelegate::OverrideManifestAssetManagerDelegate(
    const base::FilePath& override_path) {
  std::string content;
  if (!base::ReadFileToString(override_path, &content)) {
    LOG(ERROR) << "Failed to read override config file: " << override_path;
    return;
  }
  auto value = base::JSONReader::Read(content, base::JSON_PARSE_RFC);
  if (!value || !value->is_dict()) {
    LOG(ERROR) << "Failed to parse override config as JSON: " << override_path;
    return;
  }
  const base::DictValue& dict = value->GetDict();

  if (const std::string* path = dict.FindString("manifest_path")) {
    manifest_path_ = base::FilePath::FromUTF8Unsafe(*path);
  }

  const base::DictValue* components = dict.FindDict("components");
  if (!components) {
    return;
  }
  for (auto item : *components) {
    const base::DictValue* comp_dict = item.second.GetIfDict();
    if (!comp_dict) {
      continue;
    }
    for (auto version_item : *comp_dict) {
      const std::string* path_str = version_item.second.GetIfString();
      if (path_str) {
        std::string key = item.first + ":" + version_item.first;
        component_overrides_[key] = base::FilePath::FromUTF8Unsafe(*path_str);
      }
    }
  }
}

OverrideManifestAssetManagerDelegate::~OverrideManifestAssetManagerDelegate() =
    default;

base::CallbackListSubscription
OverrideManifestAssetManagerDelegate::ListenForManifestReady(
    base::RepeatingCallback<void(base::FilePath)> on_ready) {
  if (!manifest_path_.empty()) {
    on_ready.Run(manifest_path_);
  }
  return base::CallbackListSubscription();
}

void OverrideManifestAssetManagerDelegate::GetFreeDiskSpace(
    base::OnceCallback<void(std::optional<base::ByteCount>)> callback) const {
  std::move(callback).Run(base::GiB(100));
}

void OverrideManifestAssetManagerDelegate::RegisterOnDemandComponent(
    const std::string& public_key_hex,
    const std::string& target_version,
    base::WeakPtr<ManifestAssetManager> manager) {
  std::string key = public_key_hex + ":" + target_version;
  auto it = component_overrides_.find(key);
  if (it != component_overrides_.end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ManifestAssetManager::OnAssetReady, manager,
                                  public_key_hex, base::Version(target_version),
                                  it->second));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ManifestAssetManager::InstallerRegistered, manager,
                       public_key_hex, target_version, true));
  }
}

void OverrideManifestAssetManagerDelegate::Uninstall(
    const std::string& public_key_hex,
    base::WeakPtr<ManifestAssetManager> manager) {
  if (manager) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ManifestAssetManager::OnAssetUninstalled,
                                  manager, public_key_hex));
  }
}

void OverrideManifestAssetManagerDelegate::RequestUpdate(
    const std::string& public_key_hex,
    bool is_background) {
  // Do nothing
}

}  // namespace optimization_guide
