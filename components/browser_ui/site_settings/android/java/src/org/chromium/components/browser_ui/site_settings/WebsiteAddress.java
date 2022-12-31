// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.net.Uri;

import androidx.annotation.Nullable;

import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;

import java.io.Serializable;

/**
 * A pattern that matches a certain set of URLs used in content settings rules. The pattern can be
 * a fully specified origin, or just a host, or a domain name pattern.
 *
 * This is roughly equivalent to C++'s ContentSettingsPattern, though more limited.
 */
public class WebsiteAddress implements Comparable<WebsiteAddress>, Serializable {
    private final String mOriginOrHostPattern;
    private final String mOrigin;
    private final String mScheme;
    private final String mHost;
    private final boolean mOmitProtocolAndPort;

    private static final String SCHEME_SUFFIX = "://";
    private static final String ANY_SUBDOMAIN_PATTERN = "[*.]";

    /**
     * Creates a new WebsiteAddress from |originOrHostOrPattern|.
     *
     * @return A new WebsiteAddress, or null if |originOrHostOrPattern| was null or empty.
     */
    @Nullable
    public static WebsiteAddress create(String originOrHostOrPattern) {
        // TODO(mvanouwerkerk): Define the behavior of this method if a url with path, query, or
        // fragment is passed in.

        if (originOrHostOrPattern == null || originOrHostOrPattern.isEmpty()) {
            return null;
        }

        // Pattern
        if (originOrHostOrPattern.startsWith(ANY_SUBDOMAIN_PATTERN)) {
            String origin = null;
            String scheme = null;
            String host = originOrHostOrPattern.substring(ANY_SUBDOMAIN_PATTERN.length());
            boolean omitProtocolAndPort = true;
            return new WebsiteAddress(
                    originOrHostOrPattern, origin, scheme, host, omitProtocolAndPort);
        }

        // Origin
        if (originOrHostOrPattern.indexOf(SCHEME_SUFFIX) != -1) {
            Uri uri = Uri.parse(originOrHostOrPattern);
            String origin = trimTrailingBackslash(originOrHostOrPattern);
            boolean omitProtocolAndPort = UrlConstants.HTTP_SCHEME.equals(uri.getScheme())
                    && (uri.getPort() == -1 || uri.getPort() == 80);
            return new WebsiteAddress(originOrHostOrPattern, origin, uri.getScheme(), uri.getHost(),
                    omitProtocolAndPort);
        }

        // Host
        String origin = null;
        String scheme = null;
        boolean omitProtocolAndPort = true;
        return new WebsiteAddress(
                originOrHostOrPattern, origin, scheme, originOrHostOrPattern, omitProtocolAndPort);
    }

    private WebsiteAddress(String originOrHostPattern, String origin, String scheme, String host,
            boolean omitProtocolAndPort) {
        mOriginOrHostPattern = originOrHostPattern;
        mOrigin = origin;
        mScheme = scheme;
        mHost = host;
        mOmitProtocolAndPort = omitProtocolAndPort;
    }

    public String getOrigin() {
        // aaa:80 and aaa must return the same origin string.
        if (mHost != null && mOmitProtocolAndPort) {
            return UrlConstants.HTTP_URL_PREFIX + mHost;
        } else {
            return mOrigin;
        }
    }

    public String getHost() {
        return mHost;
    }

    public String getTitle() {
        if (mOrigin == null) return mHost;
        return UrlFormatter.formatUrlForSecurityDisplay(mOrigin,
                mOmitProtocolAndPort ? SchemeDisplay.OMIT_HTTP_AND_HTTPS : SchemeDisplay.SHOW);
    }

    /**
     * Returns true if {@code url} matches this WebsiteAddress's origin or host pattern.
     */
    public boolean matches(String url) {
        return WebsitePreferenceBridgeJni.get().urlMatchesContentSettingsPattern(
                url, mOriginOrHostPattern);
    }

    private String getDomainAndRegistry() {
        if (mOrigin != null) return UrlUtilities.getDomainAndRegistry(mOrigin, false);
        // getDomainAndRegistry works better having a protocol prefix.
        return UrlUtilities.getDomainAndRegistry(UrlConstants.HTTP_URL_PREFIX + mHost, false);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof WebsiteAddress) {
            WebsiteAddress other = (WebsiteAddress) obj;
            return compareTo(other) == 0;
        }
        return false;
    }

    @Override
    public int hashCode() {
        int hash = 17;
        hash = hash * 31 + (mOrigin == null ? 0 : mOrigin.hashCode());
        hash = hash * 31 + (mScheme == null ? 0 : mScheme.hashCode());
        hash = hash * 31 + (mHost == null ? 0 : mHost.hashCode());
        return hash;
    }

    @Override
    public int compareTo(WebsiteAddress to) {
        if (this == to) return 0;
        String domainAndRegistry1 = getDomainAndRegistry();
        String domainAndRegistry2 = to.getDomainAndRegistry();
        int domainComparison = domainAndRegistry1.compareTo(domainAndRegistry2);
        if (domainComparison != 0) return domainComparison;
        // The same domain. Compare by scheme for grouping sites by scheme.
        if ((mScheme == null) != (to.mScheme == null)) return mScheme == null ? -1 : 1;
        if (mScheme != null) { // && to.mScheme != null
            int schemesComparison = mScheme.compareTo(to.mScheme);
            if (schemesComparison != 0) return schemesComparison;
        }
        // Now extract subdomains and compare them RTL.
        String[] subdomains1 = getSubdomainsList();
        String[] subdomains2 = to.getSubdomainsList();
        int position1 = subdomains1.length - 1;
        int position2 = subdomains2.length - 1;
        while (position1 >= 0 && position2 >= 0) {
            int subdomainComparison = subdomains1[position1--].compareTo(subdomains2[position2--]);
            if (subdomainComparison != 0) return subdomainComparison;
        }
        return position1 - position2;
    }

    private String[] getSubdomainsList() {
        int startIndex;
        String mAddress;
        if (mOrigin != null) {
            startIndex = mOrigin.indexOf(SCHEME_SUFFIX);
            if (startIndex == -1) return new String[0];
            startIndex += SCHEME_SUFFIX.length();
            mAddress = mOrigin;
        } else {
            startIndex = 0;
            mAddress = mHost;
        }
        int endIndex = mAddress.indexOf(getDomainAndRegistry());
        return --endIndex > startIndex ? mAddress.substring(startIndex, endIndex).split("\\.")
                                       : new String[0];
    }

    private static String trimTrailingBackslash(String origin) {
        return (origin.endsWith("/")) ? origin.substring(0, origin.length() - 1) : origin;
    }
}
