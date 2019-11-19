// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_base_switches.h"

namespace switches {

// Force disables scrypt key derivation for custom passphrase. If this feature
// is enabled, scrypt will be considered as an unsupported method, and Chrome
// will not be able to access data encrypted using scrypt-derived keys (valid
// passphrases will be rejected).
const base::Feature kSyncForceDisableScryptForCustomPassphrase{
    "SyncForceDisableScryptForCustomPassphrase",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSyncE2ELatencyMeasurement = {
    "SyncE2ELatencyMeasurement", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDoNotSyncFaviconDataTypes{
    "DoNotSyncFaviconDataTypes", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
