// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

import android.support.annotation.Nullable;
import android.text.TextUtils;

/**
 * Legacy helper information meant to help with the migration process to OfflineItems.
 */
public class LegacyHelpers {
    // These are legacy namespaces for the purpose of ID generation that will only affect the UI.
    public static final String LEGACY_OFFLINE_PAGE_NAMESPACE = "LEGACY_OFFLINE_PAGE";
    public static final String LEGACY_DOWNLOAD_NAMESPACE = "LEGACY_DOWNLOAD";
    private static final String LEGACY_DOWNLOAD_NAMESPACE_PREFIX = "LEGACY_DOWNLOAD";

    /**
     * Helper to build a {@link ContentId} based on a single GUID for old offline content sources
     * (downloads and offline pages).
     * TODO(shaktisahu): Make this function aware of incognito downloads.
     * @param isOfflinePage Whether or not {@code guid} is for an offline page or a download.
     * @param guid          The {@code guid} of the download.
     * @return              A new {@link ContentId} instance.
     */
    public static ContentId buildLegacyContentId(boolean isOfflinePage, String guid) {
        String namespace =
                isOfflinePage ? LEGACY_OFFLINE_PAGE_NAMESPACE : LEGACY_DOWNLOAD_NAMESPACE;
        return new ContentId(namespace, guid);
    }

    /**
     * Helper to determine if a {@link ContentId} was created from
     * {@link #buildLegacyContentId(boolean, String)} for a download ({@code false} for {@code
     * isOfflinePage}).
     * @param id The {@link ContentId} to inspect.
     * @return   Whether or not {@code id} was built for a traditional download.
     */
    public static boolean isLegacyDownload(@Nullable ContentId id) {
        return id != null && id.namespace != null
                && id.namespace.startsWith(LEGACY_DOWNLOAD_NAMESPACE);
    }

    /**
     * Helper to determine if a {@link ContentId} was created from
     * {@link #buildLegacyContentId(boolean, String)} for an offline page ({@code true} for {@code
     * isOfflinePage}).
     * @param id The {@link ContentId} to inspect.
     * @return   Whether or not {@code id} was built for a traditional offline page.
     */
    public static boolean isLegacyOfflinePage(@Nullable ContentId id) {
        return id != null && TextUtils.equals(LEGACY_OFFLINE_PAGE_NAMESPACE, id.namespace);
    }

    private LegacyHelpers() {}
}
