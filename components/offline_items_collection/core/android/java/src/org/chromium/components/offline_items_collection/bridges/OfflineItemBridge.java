// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection.bridges;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.url.GURL;

import java.util.ArrayList;

/**
 * The Java counterpart to the C++ class OfflineItemBridge
 * (components/offline_items_collection/core/android/offline_item_bridge.h).  This class has no
 * public members or methods and is meant as a private factory to build {@link OfflineItem}
 * instances.
 */
@JNINamespace("offline_items_collection::android")
public final class OfflineItemBridge {
    private OfflineItemBridge() {}

    /**
     * This is a helper method to allow C++ to create an {@link ArrayList} to add
     * {@link OfflineItem}s to.
     * @return An {@link ArrayList} for {@link OfflineItem}s.
     */
    @CalledByNative
    private static ArrayList<OfflineItem> createArrayList() {
        return new ArrayList<OfflineItem>();
    }

    /**
     * Creates an {@link OfflineItem} from the passed in parameters.  See {@link OfflineItem} for a
     * list of the members that will be populated.  If {@code list} isn't {@code null}, the newly
     * created {@link OfflineItem} will be added to it.
     * @param list An {@link ArrayList} to optionally add the newly created {@link OfflineItem} to.
     * @return The newly created {@link OfflineItem} based on the passed in parameters.
     */
    @CalledByNative
    private static OfflineItem createOfflineItemAndMaybeAddToList(
            ArrayList<OfflineItem> list,
            String nameSpace,
            String id,
            String title,
            String description,
            @OfflineItemFilter int filter,
            boolean isTransient,
            boolean isSuggested,
            boolean isAccelerated,
            boolean promoteOrigin,
            long totalSizeBytes,
            boolean externallyRemoved,
            long creationTimeMs,
            long completionTimeMs,
            long lastAccessedTimeMs,
            boolean isOpenable,
            String filePath,
            String mimeType,
            GURL url,
            GURL originalUrl,
            boolean isOffTheRecord,
            String otrProfileId,
            GURL referrerUrl,
            boolean hasUserGesture,
            @OfflineItemState int state,
            @FailState int failState,
            @PendingState int pendingState,
            boolean isResumable,
            boolean allowMetered,
            long receivedBytes,
            long progressValue,
            long progressMax,
            @OfflineItemProgressUnit int progressUnit,
            long timeRemainingMs,
            boolean isDangerous,
            boolean canRename,
            boolean ignoreVisuals,
            double contentQualityScore) {
        OfflineItem item = new OfflineItem();
        item.id.namespace = nameSpace;
        item.id.id = id;
        item.title = title;
        item.description = description;
        item.filter = filter;
        item.isTransient = isTransient;
        item.isSuggested = isSuggested;
        item.isAccelerated = isAccelerated;
        item.promoteOrigin = promoteOrigin;
        item.totalSizeBytes = totalSizeBytes;
        item.externallyRemoved = externallyRemoved;
        item.creationTimeMs = creationTimeMs;
        item.completionTimeMs = completionTimeMs;
        item.lastAccessedTimeMs = lastAccessedTimeMs;
        item.isOpenable = isOpenable;
        item.filePath = filePath;
        item.mimeType = mimeType;
        item.url = url;
        item.originalUrl = originalUrl;
        item.isOffTheRecord = isOffTheRecord;
        item.otrProfileId = otrProfileId;
        item.referrerUrl = referrerUrl;
        item.hasUserGesture = hasUserGesture;
        item.state = state;
        item.failState = failState;
        item.pendingState = pendingState;
        item.isResumable = isResumable;
        item.allowMetered = allowMetered;
        item.receivedBytes = receivedBytes;
        item.progress =
                new OfflineItem.Progress(
                        progressValue, progressMax == -1 ? null : progressMax, progressUnit);
        item.timeRemainingMs = timeRemainingMs;
        item.isDangerous = isDangerous;
        item.canRename = canRename;
        item.ignoreVisuals = ignoreVisuals;
        item.contentQualityScore = contentQualityScore;

        if (list != null) list.add(item);
        return item;
    }

    /**
     * Creates an {@link UpdateDelta} from the passed in parameters.  See {@link UpdateDelta} for a
     * list of the members that will be populated.
     * @return The newly created {@link UpdateDelta} based on the passed in parameters.
     */
    @CalledByNative
    private static UpdateDelta createUpdateDelta(boolean stateChanged, boolean visualsChanged) {
        UpdateDelta updateDelta = new UpdateDelta();
        updateDelta.stateChanged = stateChanged;
        updateDelta.visualsChanged = visualsChanged;
        return updateDelta;
    }
}
