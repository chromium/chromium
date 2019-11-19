// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.url_formatter;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Wrapper for utilities in url_formatter.
 */
@JNINamespace("url_formatter::android")
public final class UrlFormatter {
    /**
     * Refer to url_formatter::FixupURL.
     *
     * Given a URL-like string, returns a real URL or null. For example:
     *  - "google.com" -> "http://google.com/"
     *  - "about:" -> "chrome://version/"
     *  - "//mail.google.com:/" -> "file:///mail.google.com:/"
     *  - "..." -> null
     */
    public static String fixupUrl(String uri) {
        return TextUtils.isEmpty(uri) ? null : UrlFormatterJni.get().fixupUrl(uri);
    }

    /**
     * Builds a String representation of <code>uri</code> suitable for display to the user, omitting
     * the scheme, the username and password, and trailing slash on a bare hostname.
     *
     * The IDN hostname is turned to Unicode if the Unicode representation is deemed safe.
     * For more information, see <code>url_formatter::FormatUrl(const GURL&)</code>.
     *
     * Some examples:
     *  - "http://user:password@example.com/" -> "example.com"
     *  - "https://example.com/path" -> "example.com/path"
     *  - "http://www.xn--frgbolaget-q5a.se" -> "www.färgbolaget.se"
     *
     * @param uri URI to format.
     * @return Formatted URL.
     */
    public static String formatUrlForDisplayOmitScheme(String uri) {
        return UrlFormatterJni.get().formatUrlForDisplayOmitScheme(uri);
    }

    /**
     * Builds a String representation of <code>uri</code> suitable for display to the user,
     * omitting the HTTP scheme, the username and password, trailing slash on a bare hostname,
     * and converting %20 to spaces.
     *
     * The IDN hostname is turned to Unicode if the Unicode representation is deemed safe.
     * For more information, see <code>url_formatter::FormatUrl(const GURL&)</code>.
     *
     * Example:
     *  - "http://user:password@example.com/%20test" -> "example.com/ test"
     *  - "http://user:password@example.com/" -> "example.com"
     *  - "http://www.xn--frgbolaget-q5a.se" -> "www.färgbolaget.se"
     *
     * @param uri URI to format.
     * @return Formatted URL.
     */
    public static String formatUrlForDisplayOmitHTTPScheme(String uri) {
        return UrlFormatterJni.get().formatUrlForDisplayOmitHTTPScheme(uri);
    }

    /**
     * Builds a String representation of <code>uri</code> suitable for display to the user,
     * omitting the HTTP scheme, the username and password, trailing slash on a bare hostname,
     * converting %20 to spaces, and removing trivial subdomains.
     *
     * The IDN hostname is turned to Unicode if the Unicode representation is deemed safe.
     * For more information, see <code>url_formatter::FormatUrl(const GURL&)</code>.
     *
     * Example:
     *  - "http://user:password@example.com/%20test" -> "example.com"
     *  - "http://user:password@example.com/" -> "example.com"
     *  - "http://www.xn--frgbolaget-q5a.se" -> "färgbolaget.se"
     *
     * @param uri URI to format.
     * @return Formatted URL.
     */
    public static String formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(String uri) {
        return UrlFormatterJni.get().formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(uri);
    }

    /**
     * Builds a String representation of <code>uri</code> suitable for copying to the clipboard.
     * It does not omit any components, and it performs normal escape decoding. Spaces are left
     * escaped. The IDN hostname is turned to Unicode if the Unicode representation is deemed safe.
     * For more information, see <code>url_formatter::FormatUrl(const GURL&)</code>.
     *
     * @param uri URI to format.
     * @return Formatted URL.
     */
    public static String formatUrlForCopy(String uri) {
        return UrlFormatterJni.get().formatUrlForCopy(uri);
    }

    /**
     * Builds a String that strips down |uri| to its scheme, host, and port.
     * @param uri The URI to break down.
     * @return Stripped-down String containing the essential bits of the URL, or the original URL if
     *         it fails to parse it.
     */
    public static String formatUrlForSecurityDisplay(String uri) {
        return UrlFormatterJni.get().formatUrlForSecurityDisplay(uri);
    }

    /**
     * Builds a String that strips down |uri| to its host, and port.
     * @param uri The URI to break down.
     * @return Stripped-down String containing the essential bits of the URL, or the original URL if
     *         it fails to parse it.
     */
    public static String formatUrlForSecurityDisplayOmitScheme(String uri) {
        return UrlFormatterJni.get().formatUrlForSecurityDisplayOmitScheme(uri);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        String fixupUrl(String url);
        String formatUrlForDisplayOmitScheme(String url);
        String formatUrlForDisplayOmitHTTPScheme(String url);
        String formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(String url);
        String formatUrlForCopy(String url);
        String formatUrlForSecurityDisplay(String url);
        String formatUrlForSecurityDisplayOmitScheme(String url);
    }
}
