// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Represents a group of Websites that either share the same eTLD+1 or are embedded on it. */
public class WebsiteGroup implements WebsiteEntry {
    // The common eTLD+1.
    private final String mDomainAndRegistry;
    // A list of origins associated with the eTLD+1.
    private final List<Website> mWebsites;
    // Total storage taken up by all the stored websites.
    private final long mTotalUsage;
    // Total number of cookies associated with the websites.
    private final int mCookiesCount;
    // Related Website Sets info relative to the eTLD+1.
    private RWSCookieInfo mRWSInfo;

    /**
     * Groups the websites by eTLD+1.
     *
     * @param websites A collection of {@code Website} objects representing origins.
     * @return A {@code List} of {@code WebsiteGroup} objects, each corresponding to an eTLD+1.
     */
    public static List<WebsiteEntry> groupWebsites(Collection<Website> websites) {
        // Put all the sites into an eTLD+1 -> list of origins mapping.
        Map<String, List<Website>> etldMap = new HashMap<>();
        for (Website website : websites) {
            // TODO(crbug.com/40231223): Handle partitioned storage.
            String etld = website.getAddress().getDomainAndRegistry();
            List<Website> etldSites = etldMap.get(etld);
            if (etldSites == null) {
                etldSites = new ArrayList<>();
                etldMap.put(etld, etldSites);
            }
            etldSites.add(website);
        }
        // Convert the mapping to a list of WebsiteGroup objects.
        List<WebsiteEntry> entries = new ArrayList<>();
        for (Map.Entry<String, List<Website>> etld : etldMap.entrySet()) {
            entries.add(
                    (etld.getValue().size() == 1)
                            ? etld.getValue().get(0)
                            : new WebsiteGroup(etld.getKey(), etld.getValue()));
        }
        return entries;
    }

    public WebsiteGroup(String domainAndRegistry, List<Website> websites) {
        mDomainAndRegistry = domainAndRegistry;
        mWebsites = websites;

        long totalUsage = 0;
        for (Website website : websites) {
            totalUsage += website.getTotalUsage();
            // If there's more than 1 website with RWS info in the group it's fine to override it
            // since websites are grouped by eTLD+1, and RWS info are at eTLD+1 level as well.
            if (website.getRWSCookieInfo() != null) {
                mRWSInfo = website.getRWSCookieInfo();
            }
        }
        mTotalUsage = totalUsage;

        int cookiesCount = 0;
        for (Website website : websites) {
            cookiesCount += website.getNumberOfCookies();
        }
        mCookiesCount = cookiesCount;
    }

    // WebsiteEntry implementation.

    /** Returns the title to be displayed in a user-friendly way. */
    @Override
    public String getTitleForPreferenceRow() {
        return mDomainAndRegistry;
    }

    /** Returns the URL to use for fetching the favicon: https:// + eTLD+1 is returned. */
    @Override
    public GURL getFaviconUrl() {
        return new GURL(UrlConstants.HTTPS_URL_PREFIX + mDomainAndRegistry);
    }

    @Override
    public long getTotalUsage() {
        return mTotalUsage;
    }

    @Override
    public int getNumberOfCookies() {
        return mCookiesCount;
    }

    @Override
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

    /** {@inheritDoc} */
    @Override
    public boolean isPartOfRws() {
        return getRWSInfo() != null;
    }

    /** {@inheritDoc} */
    @Override
    public String getRwsOwner() {
        return isPartOfRws() ? getRWSInfo().getOwner() : null;
    }

    /** {@inheritDoc} */
    @Override
    public int getRwsSize() {
        return isPartOfRws() ? getRWSInfo().getMembersCount() : 0;
    }

    /**
     * Some Google-affiliated domains are not allowed to delete cookies for supervised accounts.
     *
     * @return true only if every single website in the group has the deletion disabled.
     */
    @Override
    public boolean isCookieDeletionDisabled(BrowserContextHandle browserContextHandle) {
        if (mWebsites.isEmpty()) return false;
        for (Website site : mWebsites) {
            if (!site.isCookieDeletionDisabled(browserContextHandle)) {
                // At least one website is deletable, so the whole group is.
                return false;
            }
        }
        return true;
    }

    public RWSCookieInfo getRWSInfo() {
        return mRWSInfo;
    }

    public String getDomainAndRegistry() {
        return mDomainAndRegistry;
    }

    public List<Website> getWebsites() {
        return mWebsites;
    }

    /** @return whether one of the underlying origins has an associated installed app. */
    public boolean hasInstalledApp(Set<String> originsWithApps) {
        for (Website site : mWebsites) {
            if (originsWithApps.contains(site.getAddress().getOrigin())) {
                return true;
            }
        }
        return false;
    }
}
