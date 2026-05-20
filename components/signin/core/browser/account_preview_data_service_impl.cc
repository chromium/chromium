// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_service_impl.h"

namespace signin {

AccountPreviewDataServiceImpl::AccountPreviewDataServiceImpl() = default;
AccountPreviewDataServiceImpl::~AccountPreviewDataServiceImpl() = default;

std::optional<AccountPreviewData>
AccountPreviewDataServiceImpl::GetAccountPreviewData(const GaiaId& gaia_id) {
  // TODO(crbug.com/510760810): Implement the actual caching and fetching logic.
  return std::nullopt;
}

}  // namespace signin
