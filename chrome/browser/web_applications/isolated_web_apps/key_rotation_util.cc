// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/key_rotation_util.h"

#include <optional>

#include "base/containers/span.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/web_applications/model/integrity_block_data.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

namespace web_app {

namespace {

bool IntegrityBlockDataHasRotatedKey(
    base::optional_ref<const IntegrityBlockData> integrity_block_data,
    base::span<const uint8_t> rotated_key) {
  return integrity_block_data &&
         integrity_block_data->HasPublicKey(rotated_key);
}

}  // namespace

std::optional<KeyRotationData> GetKeyRotationData(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IsolationData& isolation_data) {
  const auto* kr_info =
      IwaRuntimeDataProvider::GetInstance().GetKeyRotationInfo(
          web_bundle_id.id());
  if (!kr_info) {
    return std::nullopt;
  }
  const auto& rotated_key = kr_info->public_key;

  // Checks whether `rotated_key` is contained in
  // `isolation_data.integrity_block_data`.
  const bool current_installation_has_rk = IntegrityBlockDataHasRotatedKey(
      isolation_data.integrity_block_data(), rotated_key);
  const auto& pending_update = isolation_data.pending_update_info();

  // Checks whether `rotated_key` is contained in
  // `isolation_data.pending_update_info.integrity_block_data`.
  const bool pending_update_has_rk =
      pending_update && IntegrityBlockDataHasRotatedKey(
                            pending_update->integrity_block_data, rotated_key);

  return {{.rotated_key = rotated_key,
           .current_installation_has_rk = current_installation_has_rk,
           .pending_update_has_rk = pending_update_has_rk}};
}

}  // namespace web_app
