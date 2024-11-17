// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

/** File editing information for a given origin. */
public class FileEditingInfo implements Serializable {

    public static class Grant implements Serializable {
        private final String mPath;
        private final String mDisplayName;

        public Grant(String path, String displayName) {
            mPath = path;
            mDisplayName = displayName;
        }

        public String getPath() {
            return mPath;
        }

        public String getDisplayName() {
            return mDisplayName;
        }
    }

    private final String mOrigin;
    private List<Grant> mGrants;

    public FileEditingInfo(SiteSettingsDelegate delegate, String origin) {
        mOrigin = origin;
        fetchGrants(delegate);
    }

    private void fetchGrants(SiteSettingsDelegate delegate) {
        mGrants = new ArrayList<Grant>();
        String[][] pathsAndDisplayNames = delegate.getFileSystemAccessGrants(mOrigin);
        assert pathsAndDisplayNames != null;
        assert pathsAndDisplayNames.length == 2;
        assert pathsAndDisplayNames[0].length == pathsAndDisplayNames[1].length;
        for (int i = 0; i < pathsAndDisplayNames[0].length; i++) {
            mGrants.add(new Grant(pathsAndDisplayNames[0][i], pathsAndDisplayNames[1][i]));
        }
    }

    public String getOrigin() {
        return mOrigin;
    }

    public List<Grant> getGrants() {
        return mGrants;
    }

    public void revokeGrant(SiteSettingsDelegate delegate, Grant grant) {
        delegate.revokeFileSystemAccessGrant(mOrigin, grant.getPath());
        fetchGrants(delegate);
    }
}
