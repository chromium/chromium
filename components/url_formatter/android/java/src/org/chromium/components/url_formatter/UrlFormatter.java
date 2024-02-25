// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.url_formatter;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** Wrapper for utilities in url_formatter. */
@JNINamespace("url_formatter::android")
public final class UrlFormatter {
    /**
     * Refer to url_formatter::FixupURL.
     *
     * Given a URL-like string, returns a possibly-invalid GURL. For example:
     *  - "google.com" -> "http://google.com/"
     *  - "about:" -> "chrome://version/"
     *  - "//mail.google.com:/" -> "file:///mail.google.com:/"
     *  - "0x100.0" -> "http://0x100.0/" (invalid)
     */
    public static GURL fixupUrl(String uri) {
        if (TextUtils.isEmpty(uri)) return GURL.emptyGURL();
        GURL.ensureNativeInitializedForGURL();
        return UrlFormatterJni.get().fixupUrl(uri);
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
     * omitting the HTTP/HTTPS scheme, the username and password, trailing slash on a bare hostname,
     * converting %20 to spaces, and removing trivial subdomains.
     *
     * The IDN hostname is turned to Unicode if the Unicode representation is deemed safe.
     * For more information, see <code>url_formatter::FormatUrl(const GURL&)</code>.
     *
     * Example:
     *  - "http://user:password@example.com/%20test" -> "example.com/ test"
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
     * Builds a String representation of <code>uri</code> suitable for display to the user,
     * omitting the HTTP/HTTPS scheme, the username and password, the path and removing trivial
     * subdomains.
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
    public static String formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(GURL uri) {
        return UrlFormatterJni.get().formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(uri);
    }

    /**
     * Builds a String representation of <code>uri</code> suitable for display to the user,
     * omitting the username and password and trailing slash on a bare hostname.
     *
     * The IDN hostname is turned to Unicode if the Unicode representation is deemed safe.
     * For more information, see <code>url_formatter::FormatUrl(const GURL&)</code>.
     *
     * Example:
     *  - "http://user:password@example.com/%20test" -> "http://example.com/%20test"
     *  - "http://user:password@example.com/" -> "http://example.com"
     *  - "http://www.xn--frgbolaget-q5a.se" -> "http://www.färgbolaget.se"
     *
     * @param uri URI to format.
     * @return Formatted URL.
     */
    public static String formatUrlForDisplayOmitUsernamePassword(String uri) {
        return UrlFormatterJni.get().formatUrlForDisplayOmitUsernamePassword(uri);
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
     * This is a convenience function for formatting a URL in a concise and
     * human-friendly way, to help users make security-related decisions (or in
     * other circumstances when people need to distinguish sites, origins, or
     * otherwise-simplified URLs from each other).
     *
     * Internationalized domain names (IDN) will be presented in Unicode if
     * they're regarded safe except that domain names with RTL characters
     * will still be in ACE/punycode for now (http://crbug.com/650760).
     * See http://dev.chromium.org/developers/design-documents/idn-in-google-chrome
     * for details on the algorithm.
     *
     * - Omits the path for standard schemes, excepting file and filesystem.
     * - Omits the port if it is the default for the scheme.
     *
     * Do not use this for URLs which will be parsed or sent to other applications.
     *
     * @param url The URL to format.
     * @return The formatted URL.
     */
    public static String formatUrlForSecurityDisplay(String url) {
        return UrlFormatterJni.get().formatStringUrlForSecurityDisplay(url, SchemeDisplay.SHOW);
    }

    /**
     * This is a convenience function for formatting a URL in a concise and
     * human-friendly way, to help users make security-related decisions (or in
     * other circumstances when people need to distinguish sites, origins, or
     * otherwise-simplified URLs from each other).
     *
     * Internationalized domain names (IDN) will be presented in Unicode if
     * they're regarded safe except that domain names with RTL characters
     * will still be in ACE/punycode for now (http://crbug.com/650760).
     * See http://dev.chromium.org/developers/design-documents/idn-in-google-chrome
     * for details on the algorithm.
     *
     * - Omits the path for standard schemes, excepting file and filesystem.
     * - Omits the port if it is the default for the scheme.
     *
     * Do not use this for URLs which will be parsed or sent to other applications.
     *
     * Generally, prefer SchemeDisplay.SHOW to omitting the scheme unless there is
     * plenty of indication as to whether the origin is secure elsewhere in the UX.
     * For example, in Chrome's Page Info Bubble, there are icons and strings
     * indicating origin (non-)security. But in the HTTP Basic Auth prompt (for
     * example), the scheme may be the only indicator.
     *
     * @param url The URL to format.
     * @param schemeDisplay Specifies how to display the scheme.
     * @return The formatted URL.
     */
    public static String formatUrlForSecurityDisplay(GURL url, @SchemeDisplay int schemeDisplay) {
        if (url == null) return "";
        return UrlFormatterJni.get().formatUrlForSecurityDisplay(url, schemeDisplay);
    }

    /**
     * This is a convenience function for formatting an Origin in a concise and
     * human-friendly way, to help users make security-related decisions.
     *
     * - Omits the port if it is 0 or the default for the scheme.
     *
     * Do not use this for origins which will be parsed or sent to other
     * applications.
     *
     * Generally, prefer SchemeDisplay.SHOW to omitting the scheme unless there is
     * plenty of indication as to whether the origin is secure elsewhere in the UX.
     *
     * @param origin The Origin to format.
     * @param schemeDisplay Specifies how to display the scheme.
     * @return The formatted Origin.
     */
    public static String formatOriginForSecurityDisplay(
            Origin origin, @SchemeDisplay int schemeDisplay) {
        if (origin == null) return "";
        return UrlFormatterJni.get().formatOriginForSecurityDisplay(origin, schemeDisplay);
    }

    /**
     * See {@link #formatUrlForSecurityDisplay(GURL, int)}.
     *
     * @deprecated Please use {@link #formatUrlForSecurityDisplay(GURL, int)} instead.
     */
    @Deprecated
    public static String formatUrlForSecurityDisplay(String uri, @SchemeDisplay int schemeDisplay) {
        return UrlFormatterJni.get().formatStringUrlForSecurityDisplay(uri, schemeDisplay);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        GURL fixupUrl(String url);

        String formatUrlForDisplayOmitScheme(String url);

        String formatUrlForDisplayOmitHTTPScheme(String url);

        String formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(String url);

        String formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(GURL url);

        String formatUrlForDisplayOmitUsernamePassword(String url);

        String formatUrlForCopy(String url);

        String formatUrlForSecurityDisplay(GURL url, @SchemeDisplay int schemeDisplay);

        String formatOriginForSecurityDisplay(Origin origin, @SchemeDisplay int schemeDisplay);

        String formatStringUrlForSecurityDisplay(String url, @SchemeDisplay int schemeDisplay);
    }
}
