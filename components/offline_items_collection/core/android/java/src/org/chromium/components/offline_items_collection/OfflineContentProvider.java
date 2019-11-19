// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

import org.chromium.base.Callback;

import java.util.ArrayList;

/**
 * This interface is a Java counterpart to the C++ OfflineContentProvider
 * (components/offline_items_collection/core/offline_content_provider.h) class.
 */
public interface OfflineContentProvider {
    /**
     * This interface is a Java counterpart to the C++ OfflineContentProvider::Observer
     * (components/offline_items_collection/core/offline_content_provider.h) class.
     */
    interface Observer {
        /** See OfflineContentProvider::Observer::OnItemsAdded(...). */
        void onItemsAdded(ArrayList<OfflineItem> items);

        /** See OfflineContentProvider::Observer::OnItemRemoved(...). */
        void onItemRemoved(ContentId id);

        /** See OfflineContentProvider::Observer::OnItemUpdated(...). */
        void onItemUpdated(OfflineItem item, UpdateDelta updateDelta);
    }

    /** See OfflineContentProvider::OpenItem(...). */
    void openItem(@LaunchLocation int location, ContentId id);

    /** See OfflineContentProvider::RemoveItem(...). */
    void removeItem(ContentId id);

    /** See OfflineContentProvider::CancelDownload(...). */
    void cancelDownload(ContentId id);

    /** See OfflineContentProvider::PauseDownload(...). */
    void pauseDownload(ContentId id);

    /** See OfflineContentProvider::ResumeDownload(...). */
    void resumeDownload(ContentId id, boolean hasUserGesture);

    /** See OfflineContentProvider::GetItemById(...). */
    void getItemById(ContentId id, Callback<OfflineItem> callback);

    /** See OfflineContentProvider::GetAllItems(). */
    void getAllItems(Callback<ArrayList<OfflineItem>> callback);

    /** See OfflineContentProvider::GetVisualsForItem(...). */
    void getVisualsForItem(ContentId id, VisualsCallback callback);

    /** See OfflineContentProvider::GetShareInfoForItem(...). */
    void getShareInfoForItem(ContentId id, ShareCallback callback);

    /** See OfflineContentProvider::RenameItem(...). */
    void renameItem(ContentId id, String name, Callback<Integer /*RenameResult*/> callback);

    /** See OfflineContentProvider::AddObserver(...). */
    void addObserver(Observer observer);

    /** See OfflineContentProvider::RemoveObserver(...). */
    void removeObserver(Observer observer);
}