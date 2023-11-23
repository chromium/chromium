// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import java.io.Serializable;

/** First Party Sets information for a given website. */
public class FPSCookieInfo implements Serializable {
    private final String mOwnerHost;
    private final int mMembersCount;

    public FPSCookieInfo(String ownerHost, int membersCount) {
        mOwnerHost = ownerHost;
        mMembersCount = membersCount;
    }

    public String getOwner() {
        return mOwnerHost;
    }

    public int getMembersCount() {
        return mMembersCount;
    }
}
