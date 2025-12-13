// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/encryption/cache/utils.h"

#include "base/feature_list.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/encryption/core/features.h"
#include "components/prefs/pref_service.h"

namespace enterprise_encryption {

bool ShouldEncryptHttpCache(const PrefService* prefs) {
  if (!prefs) {
    return false;
  }

  return base::FeatureList::IsEnabled(kEnableCacheEncryption) &&
         prefs->GetBoolean(enterprise_connectors::kCacheEncryptionEnabledPref);
}

}  // namespace enterprise_encryption
