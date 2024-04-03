// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

import java.io.Serializable;

/** Used by {@link WebsiteRowPreference} to display various information about one or multiple sites. */
public interface WebsiteEntry extends Serializable {
    /** @return the title to display in a {@link WebsiteRowPreference}. */
    String getTitleForPreferenceRow();

    /** @return the URL for fetching a favicon. */
    GURL getFaviconUrl();

    /** @return the total bytes used for associated storage. */
    long getTotalUsage();

    /** @return the total number of cookies associated with the entry. */
    int getNumberOfCookies();

    /**
     * @return whether either the eTLD+1 or one of the origins associated with it matches the given
     *     search query.
     */
    boolean matches(String search);

    /**
     * @return whether the {@link WebsiteEntry} is a part of RWS (related website sets).
     */
    boolean isPartOfRws();

    /**
     * @return the owner of the RWS and null if the {@link WebsiteEntry} is not part of the RWS
     *     (related website sets).
     */
    String getRwsOwner();

    /**
     * @return the size of the RWS and 0 if the {@link WebsiteEntry} is not part of the RWS (related
     *     website sets).
     */
    int getRwsSize();

    /**
     * Some Google-affiliated domains are not allowed to delete cookies for supervised accounts. If
     * the entry represents a single {@link Website}, just that origin is checked. If the entry is a
     * {@link WebsiteGroup}, checked if this holds for EVERY {@link Website} in the group.
     */
    boolean isCookieDeletionDisabled(BrowserContextHandle browserContextHandle);
}
