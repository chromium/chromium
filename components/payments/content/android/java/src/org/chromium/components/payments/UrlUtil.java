// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/** URL validity checker for web payment APIs. */
@JNINamespace("payments::android")
public class UrlUtil {
    /**
     * Checks whether the page at the given URL should be allowed to use the web payment APIs.
     * @param url The URL to check.
     * @return Whether the page is allowed to use web payment APIs.
     */
    public static boolean isOriginAllowedToUseWebPaymentApis(String url) {
        return UrlUtilJni.get().isOriginAllowedToUseWebPaymentApis(url);
    }

    /**
     * Checks whether the page at the given URL would typically be used for local development, e.g.,
     * localhost.
     * @param url The URL to check.
     * @return Whether this is a local development URL.
     */
    public static boolean isLocalDevelopmentUrl(String url) {
        return UrlUtilJni.get().isLocalDevelopmentUrl(url);
    }

    /** The interface implemented by the automatically generated JNI bindings class UrlUtilJni. */
    @NativeMethods
    /* package */ interface Natives {
        boolean isOriginAllowedToUseWebPaymentApis(String url);
        boolean isLocalDevelopmentUrl(String url);
    }
}
