// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.net.http.HttpEngine;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

@NullMarked
/**
 * Stub class useful for building HttpEngineNativeProvider against an Android SDK that does not yet
 * include the new android.net.http.ProxyOptions API.
 *
 * <p>See //components/cronet/android/java/src/org/chromium/net/impl/AndroidProxyOptions.java for
 * the real implementation. See the comment within
 * //components/cronet/android:httpengine_native_provider_java for more information.
 */
final class AndroidProxyOptions {
    @SuppressWarnings("DoNotCallSuggester")
    static void apply(
            HttpEngine.Builder backend, org.chromium.net.@Nullable ProxyOptions proxyOptions) {
        throw new UnsupportedOperationException(
                "This Cronet implementation does not support ProxyOptions");
    }

    private AndroidProxyOptions() {}
}
