// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import com.google.common.collect.ImmutableList;

import java.io.Serializable;
import java.util.List;

/** Related Website Sets information for a given website. */
public class RWSCookieInfo implements Serializable {
    private final String mOwnerHost;
    private final List<Website> mMembers;

    public RWSCookieInfo(String ownerHost, List<Website> members) {
        mOwnerHost = ownerHost;
        mMembers = members;
    }

    public String getOwner() {
        return mOwnerHost;
    }

    public int getMembersCount() {
        return mMembers != null ? mMembers.size() : 0;
    }

    public ImmutableList<Website> getMembers() {
        return ImmutableList.copyOf(mMembers);
    }
}
