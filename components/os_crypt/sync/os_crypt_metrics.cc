// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace os_crypt {

void LogEncryptionVersion(EncryptionPrefixVersion version) {
  base::UmaHistogramEnumeration("OSCrypt.EncryptionPrefixVersion", version);
}

}  // namespace os_crypt
