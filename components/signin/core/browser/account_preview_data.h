// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace signin {

struct DevicePreview {
  std::string cache_guid;
  base::Time last_updated;
  sync_pb::SyncEnums_OsType os_type =
      sync_pb::SyncEnums_OsType_OS_TYPE_UNSPECIFIED;
  sync_pb::SyncEnums_DeviceFormFactor form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED;

  bool operator==(const DevicePreview&) const = default;
};

// Holds the non-identity preview data and statistics fetched from the server
// for signed-in accounts.
struct AccountPreviewData {
  AccountPreviewData();
  AccountPreviewData(const AccountPreviewData&);
  AccountPreviewData(AccountPreviewData&&) noexcept;
  AccountPreviewData& operator=(const AccountPreviewData&);
  AccountPreviewData& operator=(AccountPreviewData&&) noexcept;
  ~AccountPreviewData();

  absl::flat_hash_map<syncer::DataType, size_t> counts;

  std::vector<DevicePreview> devices;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_H_
