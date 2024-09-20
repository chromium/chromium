// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.protocol.NigoriKey;

/**
 * Singleton that shares the explicit passphrase (e.g. custom passphrase) content with layers
 * outside of the browser which have an independent sync client, and thus separate encryption
 * infrastructure. That way, if the user has entered their passphrase in the browser, it does not
 * need to be entered again.
 *
 * <p>This class is only used from the native side, the only APIs exposed to Java are testing ones.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public abstract class ExplicitPassphrasePlatformClient {
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public abstract void setExplicitDecryptionPassphrase(
            CoreAccountInfo account, NigoriKey nigoriKey);

    @CalledByNative
    private static void setExplicitDecryptionPassphrase(CoreAccountInfo account, byte[] nigoriKey) {
        NigoriKey parsedKey;
        try {
            parsedKey = NigoriKey.parseFrom(nigoriKey);
        } catch (InvalidProtocolBufferException e) {
            assert false;
            return;
        }

        ExplicitPassphrasePlatformClient client =
                ServiceLoaderUtil.maybeCreate(ExplicitPassphrasePlatformClient.class);
        if (client != null) {
            client.setExplicitDecryptionPassphrase(account, parsedKey);
        }
    }
}
