// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

/** URL validity checker for web payment APIs. */
@JNINamespace("payments::android")
public class UrlUtil {
    /**
     * Returns false for invalid URL format or a relative URI.
     *
     * @param url The payment method name.
     * @return TRUE if given url is valid and not a relative URI.
     */
    public static boolean isURLValid(GURL url) {
        return url != null
                && url.isValid()
                && !url.getScheme().isEmpty()
                && (UrlConstants.HTTPS_SCHEME.equals(url.getScheme())
                        || UrlConstants.HTTP_SCHEME.equals(url.getScheme()));
    }

    /**
     * Checks whether the page at the given URL should be allowed to use the web payment APIs.
     * @param url The URL to check.
     * @return Whether the page is allowed to use web payment APIs.
     */
    public static boolean isOriginAllowedToUseWebPaymentApis(GURL url) {
        return UrlUtilJni.get().isOriginAllowedToUseWebPaymentApis(url);
    }

    /**
     * Checks whether the given URL is a valid payment method identifier.
     * @param url The URL to check.
     * @return Whether the given URL is a valid payment method identifier.
     */
    public static boolean isValidUrlBasedPaymentMethodIdentifier(GURL url) {
        return UrlUtilJni.get().isValidUrlBasedPaymentMethodIdentifier(url);
    }

    /**
     * Checks whether the page at the given URL would typically be used for local development, e.g.,
     * localhost.
     * @param url The URL to check.
     * @return Whether this is a local development URL.
     */
    public static boolean isLocalDevelopmentUrl(GURL url) {
        return UrlUtilJni.get().isLocalDevelopmentUrl(url);
    }

    /** The interface implemented by the automatically generated JNI bindings class UrlUtilJni. */
    @NativeMethods
    /* package */ interface Natives {
        boolean isOriginAllowedToUseWebPaymentApis(GURL url);

        boolean isValidUrlBasedPaymentMethodIdentifier(GURL url);

        boolean isLocalDevelopmentUrl(GURL url);
    }
}
