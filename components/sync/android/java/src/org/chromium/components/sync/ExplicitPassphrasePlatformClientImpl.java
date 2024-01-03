// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.protocol.NigoriKey;

/** Empty implementation for use with public code. */
class ExplicitPassphrasePlatformClientImpl extends ExplicitPassphrasePlatformClient {
    @Override
    public void setExplicitDecryptionPassphrase(CoreAccountInfo account, NigoriKey nigoriKey) {}
}
