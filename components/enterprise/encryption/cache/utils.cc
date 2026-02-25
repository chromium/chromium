// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/encryption/cache/utils.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/encryption/core/features.h"
#include "components/prefs/pref_service.h"

namespace enterprise_encryption {

bool ShouldEncryptHttpCache(const PrefService* prefs) {
  bool is_testing_flag_enabled =
      base::FeatureList::IsEnabled(kEnableCacheEncryptionForTesting);

  bool is_policy_enabled =
      prefs &&
      prefs->GetBoolean(enterprise_connectors::kCacheEncryptionEnabledPref);

  // Log intended cache encryption state.
  // TODO(crbug.com/474585860): Improve metrics to allow for better performance
  // data slicing.
  base::UmaHistogramBoolean("Enterprise.CacheEncryptionPolicyEnabled",
                            is_policy_enabled || is_testing_flag_enabled);

  return is_testing_flag_enabled ||
         (base::FeatureList::IsEnabled(kEnableCacheEncryption) &&
          is_policy_enabled);
}

}  // namespace enterprise_encryption
