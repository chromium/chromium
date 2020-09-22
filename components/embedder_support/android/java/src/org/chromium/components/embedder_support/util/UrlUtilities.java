// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.base.CollectionUtil;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.HashSet;

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
    private static final String TAG = "UrlUtilities";

    /**
     * URI schemes that are internal to Chrome.
     */
    private static final HashSet<String> INTERNAL_SCHEMES =
            CollectionUtil.newHashSet(UrlConstants.CHROME_SCHEME, UrlConstants.CHROME_NATIVE_SCHEME,
                    ContentUrlConstants.ABOUT_SCHEME);

    private static final String TEL_URL_PREFIX = "tel:";

    /**
     * @param uri A URI.
     *
     * @return True if the URI's scheme is phone number scheme.
     */
    public static boolean isTelScheme(String uri) {
        return uri != null && uri.startsWith(TEL_URL_PREFIX);
    }

    /**
     * @param uri A URI.
     *
     * @return The string after tel: scheme. Normally, it should be a phone number, but isn't
     *         guaranteed.
     */
    public static String getTelNumber(String uri) {
        if (uri == null || !uri.contains(":")) return "";
        String[] parts = uri.split(":");
        if (parts.length <= 1) return "";
        return parts[1];
    }

    /**
     * @param uri A URI.
     *
     * @return True if the URI's scheme is one that ContentView can handle.
     */
    public static boolean isAcceptedScheme(String uri) {
        return UrlUtilitiesJni.get().isAcceptedScheme(uri);
    }

    /**
     * @param uri A URI.
     *
     * @return True if the URI is valid for Intent fallback navigation.
     */
    public static boolean isValidForIntentFallbackNavigation(String uri) {
        return UrlUtilitiesJni.get().isValidForIntentFallbackNavigation(uri);
    }

    /**
     * @param uri A URI.
     *
     * @return True if the URI's scheme is one that Chrome can download.
     */
    public static boolean isDownloadableScheme(String uri) {
        return UrlUtilitiesJni.get().isDownloadable(uri);
    }

    /**
     * @param gurl A GURL.
     *
     * @return Whether the URL's scheme is for a internal chrome page.
     */
    public static boolean isInternalScheme(GURL gurl) {
        return INTERNAL_SCHEMES.contains(gurl.getScheme());
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
        return UrlUtilitiesJni.get().sameDomainOrHost(
                primaryUrl, secondaryUrl, includePrivateRegistries);
    }

    /**
     * This function works by calling net::registry_controlled_domains::GetDomainAndRegistry
     *
     * @param uri A URI
     * @param includePrivateRegistries Whether or not to consider private registries.
     *
     * @return The registered, organization-identifying host and all its registry information, but
     * no subdomains, from the given URI. Returns an empty string if the URI is invalid, has no host
     * (e.g. a file: URI), has multiple trailing dots, is an IP address, has only one subcomponent
     * (i.e. no dots other than leading/trailing ones), or is itself a recognized registry
     * identifier.
     */
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

        return parsed.getScheme() + "://" + ((parsed.getHost() != null) ? parsed.getHost() : "")
                + ((parsed.getPort() != -1) ? (":" + parsed.getPort()) : "");
    }

    /**
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

    @NativeMethods
    public interface Natives {
        boolean isDownloadable(String url);
        boolean isValidForIntentFallbackNavigation(String url);
        boolean isAcceptedScheme(String url);
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
    }
}
