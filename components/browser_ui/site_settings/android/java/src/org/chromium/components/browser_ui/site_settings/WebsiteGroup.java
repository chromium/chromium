// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Represents a group of Websites that either share the same eTLD+1 or are embedded on it.
 */
public class WebsiteGroup implements Serializable {
    // The common eTLD+1.
    private final String mDomainAndRegistry;
    // A list of origins associated with the eTLD+1.
    private final List<Website> mWebsites;
    // Total storage taken up by all the stored websites.
    private final long mTotalUsage;

    private static final String SCHEME_SUFFIX = "://";

    /**
     * Removes the scheme in a given URL, if present.
     *
     * Examples:
     * - "google.com" -> "google.com"
     * - "https://google.com" -> "google.com"
     */
    public static String omitProtocolIfPresent(String url) {
        if (url.indexOf(SCHEME_SUFFIX) == -1) return url;
        return UrlFormatter.formatUrlForDisplayOmitScheme(url);
    }

    /**
     * Groups the websites by eTLD+1.
     *
     * @param websites A collection of {@code Website} objects representing origins.
     * @return A {@code List} of {@code WebsiteGroup} objects, each corresponding to an eTLD+1.
     */
    public static List<WebsiteGroup> groupWebsites(Collection<Website> websites) {
        // Put all the sites into an eTLD+1 -> list of origins mapping.
        Map<String, List<Website>> etldMap = new HashMap<>();
        for (Website website : websites) {
            // TODO(crbug.com/1342991): Handle partitioned storage.
            String etld = website.getAddress().getDomainAndRegistry();
            List<Website> etldSites = etldMap.get(etld);
            if (etldSites == null) {
                etldSites = new ArrayList<>();
                etldMap.put(etld, etldSites);
            }
            etldSites.add(website);
        }
        // Convert the mapping to a list of WebsiteGroup objects.
        List<WebsiteGroup> groups = new ArrayList<>();
        for (Map.Entry<String, List<Website>> etld : etldMap.entrySet()) {
            groups.add(new WebsiteGroup(etld.getKey(), etld.getValue()));
        }
        return groups;
    }

    public WebsiteGroup(String domainAndRegistry, List<Website> websites) {
        mDomainAndRegistry = domainAndRegistry;
        mWebsites = websites;

        long totalUsage = 0;
        for (Website website : websites) {
            totalUsage += website.getTotalUsage();
        }
        mTotalUsage = totalUsage;
    }

    public String getDomainAndRegistry() {
        return mDomainAndRegistry;
    }

    public List<Website> getWebsites() {
        return mWebsites;
    }

    public long getTotalUsage() {
        return mTotalUsage;
    }

    public boolean hasOneOrigin() {
        return mWebsites.size() == 1;
    }

    public boolean hasOneHttpOrigin() {
        return hasOneOrigin()
                && mWebsites.get(0).getAddress().getOrigin().startsWith(
                        UrlConstants.HTTP_URL_PREFIX);
    }

    /** Returns the title to be displayed in a user-friendly way. */
    public String getTitle() {
        // If there is only one origin, return the title of that origin without the scheme.
        if (hasOneOrigin()) {
            return omitProtocolIfPresent(mWebsites.get(0).getTitle());
        } else {
            return mDomainAndRegistry;
        }
    }

    /**
     * Returns the URL to use for fetching the favicon. If only one origin is in the group, it is
     * returned; otherwise - https + eTLD+1 is returned.
     */
    public GURL getFaviconUrl() {
        return new GURL(hasOneOrigin() ? mWebsites.get(0).getAddress().getOrigin()
                                       : UrlConstants.HTTPS_URL_PREFIX + mDomainAndRegistry);
    }

    /**
     * Returns whether either the eTLD+1 or one of the origins associated with it matches the given
     * search query.
     */
    public boolean matches(String search) {
        // eTLD+1 matches.
        if (mDomainAndRegistry.contains(search)) return true;
        for (Website site : mWebsites) {
            // One of the associated origins matches.
            if (site.getTitle().contains(search)) return true;
        }
        // No matches.
        return false;
    }
}
