// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_H_

#include <optional>

#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/sync/base/data_type.h"
#include "google_apis/gaia/gaia_id.h"

class PrefRegistrySimple;

namespace signin {

// A keyed service that provides preview data and usage metrics for the
// signed-in accounts in the profile.
class AccountPreviewDataService : public KeyedService {
 public:
  struct AccountPreviewPreference {
    GaiaId gaia_id;
    std::optional<syncer::DataType> preferred_data_type;
  };

  AccountPreviewDataService() = default;
  AccountPreviewDataService(const AccountPreviewDataService&) = delete;
  AccountPreviewDataService& operator=(const AccountPreviewDataService&) =
      delete;
  ~AccountPreviewDataService() override = default;

  // Registers the preferences used by this service.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // From the list of accounts with refresh tokens, get the account that has the
  // most interesting preview data. If the preview data for accounts are not
  // fetched yet, the first account is returned by default.
  virtual AccountPreviewPreference GetPreferredAccountForPromo() const = 0;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_H_
