// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

import android.text.TextUtils;

import androidx.annotation.Nullable;

/** Legacy helper information meant to help with the migration process to OfflineItems. */
public class LegacyHelpers {
    // These are legacy namespaces for the purpose of ID generation that will only affect the UI.
    public static final String LEGACY_OFFLINE_PAGE_NAMESPACE = "LEGACY_OFFLINE_PAGE";
    public static final String LEGACY_CONTENT_INDEX_NAMESPACE = "content_index";
    public static final String LEGACY_DOWNLOAD_NAMESPACE = "LEGACY_DOWNLOAD";
    public static final String LEGACY_ANDROID_DOWNLOAD_NAMESPACE = "LEGACY_ANDROID_DOWNLOAD";

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
        return id != null
                && id.namespace != null
                && id.namespace.startsWith(LEGACY_DOWNLOAD_NAMESPACE);
    }

    /**
     * Helper to determine if a {@link ContentId} is for an content indexed item.
     * @param id The {@link ContentId} to inspect.
     * @return   Whether or not {@code id} was built for a content indexed item.
     */
    public static boolean isLegacyContentIndexedItem(@Nullable ContentId id) {
        return id != null && TextUtils.equals(LEGACY_CONTENT_INDEX_NAMESPACE, id.namespace);
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

    /**
     * Helper to determine if a {@link ContentId} corresponds to a download through android download
     * manager.
     * @param id The {@link ContentId} to inspect.
     * @return   Whether or not {@code id} was built for a android DownloadManager download.
     */
    public static boolean isLegacyAndroidDownload(@Nullable ContentId id) {
        return id != null && TextUtils.equals(LEGACY_ANDROID_DOWNLOAD_NAMESPACE, id.namespace);
    }

    private LegacyHelpers() {}
}
