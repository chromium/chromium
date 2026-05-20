// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_IMPL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_IMPL_H_

#include "components/signin/core/browser/account_preview_data_service.h"

namespace signin {

class AccountPreviewDataServiceImpl : public AccountPreviewDataService {
 public:
  AccountPreviewDataServiceImpl();
  ~AccountPreviewDataServiceImpl() override;

  std::optional<AccountPreviewData> GetAccountPreviewData(
      const GaiaId& gaia_id) override;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_IMPL_H_
