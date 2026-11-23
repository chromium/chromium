// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager_delegate.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "crypto/sha2.h"

namespace optimization_guide {

void ManifestMonitorDelegate::SetDiskSpaceCheckPriority(
    DiskSpaceCheckReason reason,
    bool user_visible) {
  const bool was_user_visible = !disk_space_priority_reasons_.empty();
  if (user_visible) {
    disk_space_priority_reasons_.Put(reason);
  } else {
    disk_space_priority_reasons_.Remove(reason);
  }
  if (was_user_visible != !disk_space_priority_reasons_.empty()) {
    UpdateDiskSpaceCheckPriority(!disk_space_priority_reasons_.empty());
  }
}

void ManifestMonitorDelegate::UpdateDiskSpaceCheckPriority(bool user_visible) {}

std::optional<std::string> GetCrxIdFromPublicKeyHex(
    std::string_view public_key_hex) {
  std::vector<uint8_t> public_key_hash;
  if (!base::HexStringToBytes(public_key_hex, &public_key_hash) ||
      public_key_hash.size() != crypto::kSHA256Length) {
    return std::nullopt;
  }
  return crx_file::id_util::GenerateIdFromHash(public_key_hash);
}

bool ManifestAssetManagerDelegate::IsConnectionMetered() const {
  return false;
}

// static
bool ManifestAssetManagerDelegate::VerifyInstallation(
    const base::FilePath& install_dir,
    const base::DictValue& manifest) {
  // TODO(crbug.com/489511499): implement proper verification logic.
  return base::PathExists(install_dir);
}

}  // namespace optimization_guide
