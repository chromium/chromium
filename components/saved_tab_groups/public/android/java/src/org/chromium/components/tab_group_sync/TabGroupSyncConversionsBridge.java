// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Token;
import org.chromium.url.GURL;

/**
 * Java counterpart to the C++ TabGroupSyncConversionsBridge class. This class has no public members
 * or methods and is meant as a private factory to build {@link SavedTabGroup} instances.
 */
@JNINamespace("tab_groups")
public class TabGroupSyncConversionsBridge {

    /**
     * Conversion method to create a native SavedTabGroup from its Java counterpart.
     *
     * @param savedTabGroup The Java version of the {@link SavedTabGroup}.
     * @return The native pointer to the SavedTabGroup created after conversion.
     */
    public static long convertToNativeSavedTabGroup(SavedTabGroup savedTabGroup) {
        // Construct a group in native.
        long nativeSavedTabGroupPtr =
                TabGroupSyncConversionsBridgeJni.get().createGroup(savedTabGroup.localId);
        assert savedTabGroup.localId != null
                : "Java-to-native conversion expects a non-null local tab group ID";

        // Set visuals on the native group based on the Java group.
        TabGroupSyncConversionsBridgeJni.get()
                .updateVisualData(nativeSavedTabGroupPtr, savedTabGroup.title, savedTabGroup.color);

        // Add tabs to the native group.
        for (int i = 0; i < savedTabGroup.savedTabs.size(); i++) {
            SavedTabGroupTab savedTab = savedTabGroup.savedTabs.get(i);
            assert savedTab.localId != null
                    : "Java-to-native conversion expects a non-null local tab ID";
            TabGroupSyncConversionsBridgeJni.get()
                    .addTab(nativeSavedTabGroupPtr, savedTab.localId, savedTab.title, savedTab.url);
        }

        // Return the native group ptr.
        return nativeSavedTabGroupPtr;
    }

    @CalledByNative
    private static SavedTabGroup createGroup(
            String syncId,
            LocalTabGroupId localId,
            String title,
            int color,
            long creationTimeMs,
            long updateTimeMs,
            String creatorCacheGuid,
            String lastUpdaterCacheGuid,
            String collaborationId) {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = syncId;
        group.localId = localId;
        group.title = title;
        group.color = color;
        assert group.color != -1;
        group.creationTimeMs = creationTimeMs;
        group.updateTimeMs = updateTimeMs;
        group.creatorCacheGuid = creatorCacheGuid;
        group.lastUpdaterCacheGuid = lastUpdaterCacheGuid;
        group.collaborationId = collaborationId;
        return group;
    }

    @CalledByNative
    private static SavedTabGroupTab createTabAndMaybeAddToGroup(
            String syncId,
            int localId,
            String syncGroupId,
            int position,
            GURL url,
            String title,
            long creationTimeMs,
            long updateTimeMs,
            String creatorCacheGuid,
            String lastUpdaterCacheGuid,
            SavedTabGroup group) {
        SavedTabGroupTab tab = new SavedTabGroupTab();
        tab.syncId = syncId;
        tab.localId = localId == -1 ? null : localId;
        tab.syncGroupId = syncGroupId;
        tab.position = position == -1 ? null : position;
        tab.url = url;
        tab.title = title;
        tab.creationTimeMs = creationTimeMs;
        tab.updateTimeMs = updateTimeMs;
        tab.creatorCacheGuid = creatorCacheGuid;
        tab.lastUpdaterCacheGuid = lastUpdaterCacheGuid;
        if (group != null) {
            group.savedTabs.add(tab);
        }
        return tab;
    }

    @CalledByNative
    private static LocalTabGroupId createJavaTabGroupId(Token groupId) {
        assert groupId != null;
        return new LocalTabGroupId(groupId);
    }

    @CalledByNative
    private static Token getNativeTabGroupId(LocalTabGroupId tabGroupId) {
        assert tabGroupId != null;
        return tabGroupId.tabGroupId;
    }

    @NativeMethods
    interface Natives {
        long createGroup(LocalTabGroupId groupId);

        void updateVisualData(long groupPtr, String title, int color);

        void addTab(long groupPtr, int tabId, String title, GURL url);
    }
}
