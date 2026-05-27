// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "components/sync/base/data_type.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace signin {

// TODO(crbug.com/510760810): Finalize the fields of this structure based on the
// finalized server proto response. Current fields are placeholders.
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

  // Example preview data: urls for which the account has saved data.
  std::vector<std::string> password_domains;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_H_
