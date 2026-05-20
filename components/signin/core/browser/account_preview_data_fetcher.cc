// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_fetcher.h"

#include "base/functional/bind.h"
#include "components/signin/core/browser/account_preview_data.h"

namespace signin {

AccountPreviewDataFetcher::AccountPreviewDataFetcher(
    const GaiaId& gaia_id,
    IdentityManager* identity_manager,
    FetchCompleteCallback callback)
    : gaia_id_(gaia_id), callback_(std::move(callback)) {
  // Minimal implementation: immediately return empty data via callback.
  // TODO(crbug.com/510760810): Implement real network fetch in CL 3.
  std::move(callback_).Run(gaia_id_, AccountPreviewData());
}

AccountPreviewDataFetcher::~AccountPreviewDataFetcher() = default;

}  // namespace signin
