// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.protocol.NigoriKey;

/**
 * Singleton that shares the explicit passphrase (e.g. custom passphrase) content with layers
 * outside of the browser which have an independent sync client, and thus separate encryption
 * infrastructure. That way, if the user has entered their passphrase in the browser, it does not
 * need to be entered again.
 */
abstract class ExplicitPassphrasePlatformClient {
    abstract void setExplicitDecryptionPassphrase(CoreAccountInfo account, NigoriKey nigoriKey);
}
