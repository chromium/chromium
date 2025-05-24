// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import com.google.common.collect.ImmutableList;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.Serializable;
import java.util.List;

/** Related Website Sets information for a given website. */
@NullMarked
public class RwsCookieInfo implements Serializable {
    private final String mOwnerHost;
    // List of all members, including member variations with different subdomains, used to delete
    // data for an entire RWS
    // Example: www.example.com, example.com
    private final List<Website> mMembers;
    // List of all members, grouped by domain, used to display data about the RWS
    // Example: example.com
    private final List<WebsiteEntry> mMembersGroupedByDomain;

    public RwsCookieInfo(String ownerHost, List<Website> members) {
        mOwnerHost = ownerHost;
        mMembers = members;
        mMembersGroupedByDomain = WebsiteGroup.groupWebsites(members);
    }

    public String getOwner() {
        return mOwnerHost;
    }

    public @Nullable Website findWebsiteForOrigin(String origin) {
        for (Website site : mMembers) {
            if (site.getAddress().matches(origin)) {
                return site;
            }
        }
        // Ordinarily should not be reachable. May happen if the user tries to open RWS settings
        // before the website has saved any cookies.
        return null;
    }

    public int getMembersCount() {
        return mMembers != null ? mMembers.size() : 0;
    }

    public ImmutableList<Website> getMembers() {
        return ImmutableList.copyOf(mMembers);
    }

    public ImmutableList<WebsiteEntry> getMembersGroupedByDomain() {
        return ImmutableList.copyOf(mMembersGroupedByDomain);
    }
}
