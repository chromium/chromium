// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.url.GURL;

/** Helper for origin security. */
@JNINamespace("payments")
public class OriginSecurityChecker {
    /**
     * Returns true for a valid URL from a secure origin, e.g., http://localhost,
     * file:///home/user/test.html, https://bobpay.com.
     *
     * @param url The URL to check.
     * @return Whether the origin of the URL is secure.
     */
    public static boolean isOriginSecure(GURL url) {
        return OriginSecurityCheckerJni.get().isOriginSecure(url);
    }

    /**
     * Returns true for a valid URL with a cryptographic scheme, e.g., HTTPS, WSS.
     *
     * @param url The URL to check.
     * @return Whether the scheme of the URL is cryptographic.
     */
    public static boolean isSchemeCryptographic(GURL url) {
        return OriginSecurityCheckerJni.get().isSchemeCryptographic(url);
    }

    private OriginSecurityChecker() {}

    @NativeMethods
    interface Natives {
        boolean isOriginSecure(GURL url);

        boolean isSchemeCryptographic(GURL url);
    }
}
