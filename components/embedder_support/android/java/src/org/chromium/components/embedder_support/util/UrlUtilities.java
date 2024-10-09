// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.core.text.BidiFormatter;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.CollectionUtil;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.regex.Pattern;

/**
 * Utilities for working with URIs (and URLs). These methods may be used in security-sensitive
 * contexts (after all, origins are the security boundary on the web), and so the correctness bar
 * must be high.
 *
 * Use ShadowUrlUtilities to mock out native-dependent methods in tests.
 * TODO(pshmakov): we probably should just make those methods non-static.
 */
@JNINamespace("embedder_support")
public class UrlUtilities {
    /** Regular expression for prefixes to strip from publisher hostnames. */
    private static final Pattern HOSTNAME_PREFIX_PATTERN =
            Pattern.compile("^(www[0-9]*|web|ftp|wap|home|mobile|amp)\\.");

    private static final List<String> SUPPORTED_SCHEMES =
            new ArrayList<String>(
                    Arrays.asList(
                            ContentUrlConstants.ABOUT_SCHEME,
                            UrlConstants.DATA_SCHEME,
                            UrlConstants.FILE_SCHEME,
                            UrlConstants.HTTP_SCHEME,
                            UrlConstants.HTTPS_SCHEME,
                            UrlConstants.INLINE_SCHEME,
                            UrlConstants.JAVASCRIPT_SCHEME));

    private static final List<String> DOWNLOADABLE_SCHEMES =
            new ArrayList<String>(
                    Arrays.asList(
                            UrlConstants.DATA_SCHEME,
                            UrlConstants.BLOB_SCHEME,
                            UrlConstants.FILE_SCHEME,
                            UrlConstants.FILESYSTEM_SCHEME,
                            UrlConstants.HTTP_SCHEME,
                            UrlConstants.HTTPS_SCHEME));

    /** URI schemes that are internal to Chrome. */
    private static final HashSet<String> INTERNAL_SCHEMES =
            CollectionUtil.newHashSet(
                    UrlConstants.CHROME_SCHEME,
                    UrlConstants.CHROME_NATIVE_SCHEME,
                    ContentUrlConstants.ABOUT_SCHEME);

    private static final String TEL_SCHEME = "tel";

    /**
     * @param uri A URI.
     *
     * @return True if the URI's scheme is phone number scheme.
     */
    public static boolean isTelScheme(GURL gurl) {
        return gurl != null && gurl.getScheme().equals(TEL_SCHEME);
    }

    /**
     * @param uri A URI.
     *
     * @return The string after tel: scheme. Normally, it should be a phone number, but isn't
     *         guaranteed.
     */
    public static String getTelNumber(GURL gurl) {
        if (GURL.isEmptyOrInvalid(gurl)) return "";
        if (!isTelScheme(gurl)) return "";
        return gurl.getPath();
    }

    /**
     * @param url A GURL.
     *
     * @return True if the GURL's scheme is one that ContentView can handle.
     */
    public static boolean isAcceptedScheme(GURL url) {
        if (GURL.isEmptyOrInvalid(url)) return false;
        return SUPPORTED_SCHEMES.contains(url.getScheme());
    }

    /**
     * @param url A GURL.
     *
     * @return True if the GURL's scheme is one that Chrome can download.
     */
    public static boolean isDownloadableScheme(@NonNull GURL url) {
        if (!url.isValid()) return false;
        return DOWNLOADABLE_SCHEMES.contains(url.getScheme());
    }

    /**
     * @param gurl A GURL.
     *
     * @return Whether the URL's scheme is for a internal chrome page.
     */
    public static boolean isInternalScheme(@NonNull GURL gurl) {
        return INTERNAL_SCHEMES.contains(gurl.getScheme());
    }

    /** Returns whether the scheme represented by the given string is for a internal chrome page. */
    public static boolean isInternalScheme(String scheme) {
        return INTERNAL_SCHEMES.contains(scheme);
    }

    /**
     * @param url A URL.
     *
     * @return Whether the URL's scheme is HTTP or HTTPS.
     */
    public static boolean isHttpOrHttps(@NonNull GURL url) {
        return isSchemeHttpOrHttps(url.getScheme());
    }

    /**
     * @param url A URL.
     *
     * @return Whether the URL's scheme is HTTP or HTTPS.
     */
    public static boolean isHttpOrHttps(@NonNull String url) {
        // URI#getScheme would throw URISyntaxException if the other parts contain invalid
        // characters. For example, "http://foo.bar/has[square].html" has [] in the path, which
        // is not valid in URI. Both Uri.parse().getScheme() and URL().getProtocol() work in
        // this case.
        //
        // URL().getProtocol() throws MalformedURLException if the scheme is "invalid",
        // including common ones like "about:", "javascript:", "data:", etc.
        return isSchemeHttpOrHttps(Uri.parse(url).getScheme());
    }

    private static boolean isSchemeHttpOrHttps(String scheme) {
        return UrlConstants.HTTP_SCHEME.equals(scheme) || UrlConstants.HTTPS_SCHEME.equals(scheme);
    }

    /**
     * Determines whether or not the given URLs belong to the same broad domain or host.
     * "Broad domain" is defined as the TLD + 1 or the host.
     *
     * For example, the TLD + 1 for http://news.google.com would be "google.com" and would be shared
     * with other Google properties like http://finance.google.com.
     *
     * If {@code includePrivateRegistries} is marked as true, then private domain registries (like
     * appspot.com) are considered "effective TLDs" -- all subdomains of appspot.com would be
     * considered distinct (effective TLD = ".appspot.com" + 1).
     * This means that http://chromiumreview.appspot.com and http://example.appspot.com would not
     * belong to the same host.
     * If {@code includePrivateRegistries} is false, all subdomains of appspot.com
     * would be considered to be the same domain (TLD = ".com" + 1).
     *
     * @param primaryUrl First URL
     * @param secondaryUrl Second URL
     * @param includePrivateRegistries Whether or not to consider private registries.
     * @return True iff the two URIs belong to the same domain or host.
     */
    public static boolean sameDomainOrHost(
            String primaryUrl, String secondaryUrl, boolean includePrivateRegistries) {
        return UrlUtilitiesJni.get()
                .sameDomainOrHost(primaryUrl, secondaryUrl, includePrivateRegistries);
    }

    /**
     * Returns a new URL without the port in the hostname if it was present.
     *
     * @param url The url to process.
     */
    // TODO(crbug.com/40549331): Expose GURL::Replacements to Java.
    public static GURL clearPort(GURL url) {
        if (url == null || TextUtils.isEmpty(url.getPort())) return url;
        return UrlUtilitiesJni.get().clearPort(url);
    }

    /**
     * This function works by calling net::registry_controlled_domains::GetDomainAndRegistry
     *
     * @param uri A URI
     * @param includePrivateRegistries Whether or not to consider private registries.
     * @return The registered, organization-identifying host and all its registry information, but
     *     no subdomains, from the given URI. Returns an empty string if the URI is invalid, has no
     *     host (e.g. a file: URI), has multiple trailing dots, is an IP address, has only one
     *     subcomponent (i.e. no dots other than leading/trailing ones), or is itself a recognized
     *     registry identifier.
     */
    // TODO(crbug.com/40549331): Convert to GURL.
    public static String getDomainAndRegistry(String uri, boolean includePrivateRegistries) {
        if (TextUtils.isEmpty(uri)) return uri;
        return UrlUtilitiesJni.get().getDomainAndRegistry(uri, includePrivateRegistries);
    }

    /** Returns whether a URL is within another URL's scope. */
    public static boolean isUrlWithinScope(String url, String scopeUrl) {
        return UrlUtilitiesJni.get().isUrlWithinScope(url, scopeUrl);
    }

    /** @return whether two URLs match, ignoring the #fragment. */
    public static boolean urlsMatchIgnoringFragments(String url, String url2) {
        if (TextUtils.equals(url, url2)) return true;
        return UrlUtilitiesJni.get().urlsMatchIgnoringFragments(url, url2);
    }

    /** @return whether the #fragmant differs in two URLs. */
    public static boolean urlsFragmentsDiffer(String url, String url2) {
        if (TextUtils.equals(url, url2)) return false;
        return UrlUtilitiesJni.get().urlsFragmentsDiffer(url, url2);
    }

    /**
     * @param url An HTTP or HTTPS URL.
     * @return The URL without path and query.
     */
    public static String stripPath(String url) {
        assert isHttpOrHttps(url);
        Uri parsed = Uri.parse(url);

        return parsed.getScheme()
                + "://"
                + ((parsed.getHost() != null) ? parsed.getHost() : "")
                + ((parsed.getPort() != -1) ? (":" + parsed.getPort()) : "");
    }

    /**
     * TODO(crbug.com/40549331): This should use UrlFormatter, or GURL machinery.
     *
     * @param url An HTTP or HTTPS URL.
     * @return The URL without the scheme.
     */
    public static String stripScheme(String url) {
        String noScheme = url.trim();
        if (noScheme.startsWith(UrlConstants.HTTPS_URL_PREFIX)) {
            noScheme = noScheme.substring(8);
        } else if (noScheme.startsWith(UrlConstants.HTTP_URL_PREFIX)) {
            noScheme = noScheme.substring(7);
        }
        return noScheme;
    }

    /**
     * Escapes characters in text suitable for use as a query parameter value.
     * This method calls into base::EscapeQueryParamValue.
     * @param text string to be escaped.
     * @param usePlus whether or not to use "+" in place of spaces.
     * @return the escaped string.
     */
    public static String escapeQueryParamValue(String text, boolean usePlus) {
        return UrlUtilitiesJni.get().escapeQueryParamValue(text, usePlus);
    }

    /**
     * This variation of #isNtpUrl is for already parsed URLs, not for direct use on user-provided
     * url input. Do not do isNtpUrl(new GURL(user_string)), as this will not handle legacy schemes
     * like about: correctly. You should use {@link #isNtpUrl(String)} instead, or call {@link
     * UrlFormatter#fixupUrl(String)} to create the GURL instead.
     *
     * @param gurl The GURL to check whether it is for the NTP.
     * @return Whether the passed in URL is used to render the NTP.
     */
    public static boolean isNtpUrl(GURL gurl) {
        if (!gurl.isValid() || !isInternalScheme(gurl)) return false;
        return UrlConstants.NTP_HOST.equals(gurl.getHost())
                || UrlConstants.NEW_TAB_PAGE_URL_LEGACY.equals(gurl.getValidSpecOrEmpty());
    }

    /**
     * @param url The URL to check whether it is for the NTP.
     * @return Whether the passed in URL is used to render the NTP.
     * @deprecated For URLs coming from c++, those URLs should passed around in Java as a GURL. For
     *     URLs created in Java, coming from third parties or users, those URLs should be parsed
     *     into a GURL at their source using {@link UrlFormatter#fixupUrl(String)}.
     */
    @Deprecated
    public static boolean isNtpUrl(String url) {
        // Also handle the legacy chrome://newtab and about:newtab URLs since they will redirect to
        // chrome-native://newtab natively.
        if (TextUtils.isEmpty(url)) return false;
        // We need to fixup the URL to handle about: schemes and transform them into the equivalent
        // chrome:// scheme so that GURL parses the host correctly.
        GURL gurl = UrlFormatter.fixupUrl(url);
        return isNtpUrl(gurl);
    }

    /**
     * Returns whether the url matches an NTP URL exactly. This is used to support features showing
     * the omnibox before native is loaded. Prefer using {@see #isNtpUrl(GURL gurl)} when native is
     * loaded.
     *
     * @param url The current URL to compare.
     * @return Whether the given URL matches the NTP urls exactly.
     */
    public static boolean isCanonicalizedNtpUrl(String url) {
        // TODO(crbug.com/40204389): Let callers check if the library is initialized and make them
        // call this method only before native is initialized.
        // After native initialization, the homepage url could become
        // "chrome://newtab/#most_visited" on carrier phones. Simply comparing the text of the URL
        // returns a wrong result, but isNtpUrl(url) which checks the host of the URL works. See
        // https://crbug.com/1266625.
        if (LibraryLoader.getInstance().isInitialized()) return isNtpUrl(url);
        return TextUtils.equals(url, UrlConstants.NTP_URL)
                || TextUtils.equals(url, UrlConstants.NTP_NON_NATIVE_URL)
                || TextUtils.equals(url, UrlConstants.NTP_ABOUT_URL);
    }

    public static String extractPublisherFromPublisherUrl(GURL publisherUrl) {
        String publisher =
                UrlFormatter.formatUrlForDisplayOmitScheme(publisherUrl.getOrigin().getSpec());

        String trimmedPublisher = HOSTNAME_PREFIX_PATTERN.matcher(publisher).replaceFirst("");
        return BidiFormatter.getInstance().unicodeWrap(trimmedPublisher);
    }

    /**
     * See native url_util::GetValueForKeyInQuery().
     *
     * Equivalent to {@link Uri#getQueryParameter(String)}.
     *
     * @return null if the key doesn't exist in the query string for the URL. Otherwise, returns the
     * value for the key in the query string.
     */
    public static String getValueForKeyInQuery(GURL url, String key) {
        return UrlUtilitiesJni.get().getValueForKeyInQuery(url, key);
    }

    /** @return true if |url|'s scheme is for an Android intent. */
    public static boolean hasIntentScheme(GURL url) {
        return url.getScheme().equals(UrlConstants.APP_INTENT_SCHEME)
                || url.getScheme().equals(UrlConstants.INTENT_SCHEME);
    }

    @NativeMethods
    public interface Natives {
        boolean sameDomainOrHost(
                String primaryUrl, String secondaryUrl, boolean includePrivateRegistries);

        String getDomainAndRegistry(String url, boolean includePrivateRegistries);

        /** Returns whether the given URL uses the Google.com domain. */
        boolean isGoogleDomainUrl(String url, boolean allowNonStandardPort);

        /** Returns whether the given URL is a Google.com domain or sub-domain. */
        boolean isGoogleSubDomainUrl(String url);

        /** Returns whether the given URL is a Google.com Search URL. */
        boolean isGoogleSearchUrl(String url);

        /** Returns whether the given URL is the Google Web Search URL. */
        boolean isGoogleHomePageUrl(String url);

        boolean isUrlWithinScope(String url, String scopeUrl);

        boolean urlsMatchIgnoringFragments(String url, String url2);

        boolean urlsFragmentsDiffer(String url, String url2);

        String escapeQueryParamValue(String url, boolean usePlus);

        String getValueForKeyInQuery(GURL url, String key);

        GURL clearPort(GURL url);
    }
}
