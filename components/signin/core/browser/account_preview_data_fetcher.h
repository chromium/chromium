// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_FETCHER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "google_apis/gaia/gaia_id.h"

namespace signin {

class IdentityManager;
struct AccountPreviewData;

class AccountPreviewDataFetcher {
 public:
  using FetchCompleteCallback =
      base::OnceCallback<void(const GaiaId&,
                              std::optional<AccountPreviewData>)>;

  AccountPreviewDataFetcher(const GaiaId& gaia_id,
                            IdentityManager* identity_manager,
                            FetchCompleteCallback callback);
  ~AccountPreviewDataFetcher();

 private:
  GaiaId gaia_id_;
  FetchCompleteCallback callback_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_FETCHER_H_
