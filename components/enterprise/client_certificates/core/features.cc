// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/features.h"

namespace client_certificates::features {

BASE_FEATURE(kManagedClientCertificateForUserEnabled,
             "ManagedClientCertificateForUserEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsManagedClientCertificateForUserEnabled() {
  return base::FeatureList::IsEnabled(kManagedClientCertificateForUserEnabled);
}

}  // namespace client_certificates::features
