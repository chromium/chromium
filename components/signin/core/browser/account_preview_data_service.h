// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "google_apis/gaia/gaia_id.h"

class PrefRegistrySimple;

namespace signin {

// Represents the quartile tier classification of sync data count relative to
// thresholds.
// These values are persisted to logs/prefs. Entries should not be renumbered
// and numeric values should never be reused.
enum class SyncDataQuartile {
  kZero = 0,
  kBelowQ1 = 1,     // 0 < count < Q1
  kQ1ToMedian = 2,  // Q1 <= count < Median
  kMedianToQ3 = 3,  // Median <= count < Q3
  kAboveQ3 = 4,     // count >= Q3

  kMaxValue = kAboveQ3,
};

// Functions used to persist/retrieve SyncDataQuartile from/to prefs.
std::optional<SyncDataQuartile> SyncDataQuartileFromValue(int value);
int SyncDataQuartileToValue(SyncDataQuartile quartile);

struct PreferredDataTypeInfo {
  syncer::DataType data_type = syncer::UNSPECIFIED;
  SyncDataQuartile quartile = SyncDataQuartile::kZero;

  bool is_above_or_at_median() const {
    return quartile >= SyncDataQuartile::kMedianToQ3;
  }

  bool operator==(const PreferredDataTypeInfo&) const = default;
};

// A keyed service that provides preview data and usage metrics for the
// signed-in accounts in the profile.
class AccountPreviewDataService : public KeyedService {
 public:
  struct AccountPreviewPreference {
    GaiaId gaia_id;
    std::vector<PreferredDataTypeInfo> preferred_data_types;
    sync_pb::SyncEnums_DeviceFormFactor other_device_form_factor =
        sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED;

    bool operator==(const AccountPreviewPreference&) const = default;
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
  // fetched yet, the first account is returned by default. Returns std::nullopt
  // if no signed-in accounts exist.
  virtual std::optional<AccountPreviewPreference> GetPreferredAccountForPromo()
      const = 0;

  // Computes the preview preference for a single account specified by
  // `gaia_id`. If data is already cached or the account is invalid/tokens
  // unloaded, `callback` is invoked synchronously with the result. Otherwise, a
  // network fetch is started and `callback` is invoked asynchronously upon
  // completion.
  virtual void GetPreviewPreferenceForAccount(
      const GaiaId& gaia_id,
      base::OnceCallback<void(std::optional<AccountPreviewPreference>)>
          callback) = 0;

#if BUILDFLAG(IS_ANDROID)
  // Updates the account currently used by the external 1P app. A null/empty
  // value means that no account is signed-in in the app.
  virtual void UpdateExternalAppAccount(
      const std::optional<std::string>& email) = 0;
#endif
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_H_
