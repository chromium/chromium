// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Java side of the JNI bridge between TabGroupSyncServiceImpl in Java and C++. All method calls are
 * delegated to the native C++ class.
 */
@JNINamespace("tab_groups")
@NullMarked
public class TabGroupSyncServiceImpl implements TabGroupSyncService {
    private final ObserverList<TabGroupSyncService.Observer> mObservers = new ObserverList<>();
    private long mNativePtr;
    private boolean mInitialized;
    private boolean mIsObservingLocalChanges;

    @CalledByNative
    private static TabGroupSyncServiceImpl create(long nativePtr) {
        return new TabGroupSyncServiceImpl(nativePtr);
    }

    private TabGroupSyncServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
        mIsObservingLocalChanges = true;
    }

    @Override
    public void addObserver(TabGroupSyncService.Observer observer) {
        mObservers.addObserver(observer);

        // If initialization is already complete, notify the newly added observer.
        if (mInitialized) {
            observer.onInitialized();
        }
    }

    @Override
    public void removeObserver(TabGroupSyncService.Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void addGroup(SavedTabGroup savedTabGroup) {
        if (mNativePtr == 0) return;
        assert savedTabGroup != null;
        assert savedTabGroup.localId != null;
        TabGroupSyncServiceImplJni.get().addGroup(mNativePtr, savedTabGroup);
    }

    @Override
    public void removeGroup(LocalTabGroupId localTabGroupId) {
        if (mNativePtr == 0) return;
        assert localTabGroupId != null;
        TabGroupSyncServiceImplJni.get().removeGroupByLocalId(mNativePtr, localTabGroupId);
    }

    @Override
    public void removeGroup(String syncTabGroupId) {
        if (mNativePtr == 0) return;
        assert syncTabGroupId != null;
        TabGroupSyncServiceImplJni.get().removeGroupBySyncId(mNativePtr, syncTabGroupId);
    }

    @Override
    public void updateVisualData(LocalTabGroupId groupId, String title, int color) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get().updateVisualData(mNativePtr, groupId, title, color);
    }

    @Override
    public void makeTabGroupShared(
            LocalTabGroupId tabGroupId,
            String collaborationId,
            @Nullable Callback<Boolean> tabGroupSharingCallback) {
        if (mNativePtr == 0) return;
        assert tabGroupId != null;
        TabGroupSyncServiceImplJni.get()
                .makeTabGroupShared(
                        mNativePtr, tabGroupId, collaborationId, tabGroupSharingCallback);
    }

    @Override
    public void aboutToUnShareTabGroup(LocalTabGroupId tabGroupId, @Nullable Callback<Boolean> callback) {
        if (mNativePtr == 0) return;
        assert tabGroupId != null;
        TabGroupSyncServiceImplJni.get().aboutToUnShareTabGroup(mNativePtr, tabGroupId, callback);
    }

    @Override
    public void onTabGroupUnShareComplete(LocalTabGroupId tabGroupId, boolean success) {
        if (mNativePtr == 0) return;
        assert tabGroupId != null;
        TabGroupSyncServiceImplJni.get().onTabGroupUnShareComplete(mNativePtr, tabGroupId, success);
    }

    @Override
    public void addTab(LocalTabGroupId groupId, int tabId, String title, GURL url, int position) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get().addTab(mNativePtr, groupId, tabId, title, url, position);
    }

    @Override
    public void updateTab(
            LocalTabGroupId groupId, int tabId, String title, GURL url, int position) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get()
                .updateTab(mNativePtr, groupId, tabId, title, url, position);
    }

    @Override
    public void removeTab(LocalTabGroupId groupId, int tabId) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get().removeTab(mNativePtr, groupId, tabId);
    }

    @Override
    public void moveTab(LocalTabGroupId groupId, int tabId, int newIndexInGroup) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get().moveTab(mNativePtr, groupId, tabId, newIndexInGroup);
    }

    @Override
    public void onTabSelected(@Nullable LocalTabGroupId groupId, int tabId, String tabTitle) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get().setTabSelected(mNativePtr, groupId, tabId, tabTitle);
    }

    @Override
    public String[] getAllGroupIds() {
        if (mNativePtr == 0) return new String[0];
        return TabGroupSyncServiceImplJni.get().getAllGroupIds(mNativePtr);
    }

    @Override
    public @Nullable SavedTabGroup getGroup(String syncGroupId) {
        if (mNativePtr == 0) return null;
        return TabGroupSyncServiceImplJni.get().getGroupBySyncGroupId(mNativePtr, syncGroupId);
    }

    @Override
    public SavedTabGroup getGroup(LocalTabGroupId localGroupId) {
        assert localGroupId != null;
        return TabGroupSyncServiceImplJni.get().getGroupByLocalGroupId(mNativePtr, localGroupId);
    }

    @Override
    public void updateLocalTabGroupMapping(
            String syncId, LocalTabGroupId localId, @OpeningSource int openingSource) {
        if (mNativePtr == 0) return;
        assert localId != null;
        TabGroupSyncServiceImplJni.get()
                .updateLocalTabGroupMapping(mNativePtr, syncId, localId, openingSource);
    }

    @Override
    public void removeLocalTabGroupMapping(
            LocalTabGroupId localId, @ClosingSource int closingSource) {
        if (mNativePtr == 0) return;
        assert localId != null;
        TabGroupSyncServiceImplJni.get()
                .removeLocalTabGroupMapping(mNativePtr, localId, closingSource);
    }

    @Override
    public List<LocalTabGroupId> getDeletedGroupIds() {
        if (mNativePtr == 0) return new ArrayList<>();
        List<LocalTabGroupId> deletedIds = new ArrayList<>();
        Object[] objects = TabGroupSyncServiceImplJni.get().getDeletedGroupIds(mNativePtr);
        for (Object obj : objects) {
            assert obj instanceof LocalTabGroupId;
            deletedIds.add((LocalTabGroupId) obj);
        }
        return deletedIds;
    }

    @Override
    public void updateLocalTabId(LocalTabGroupId localGroupId, String syncTabId, int localTabId) {
        if (mNativePtr == 0) return;
        assert localGroupId != null;
        TabGroupSyncServiceImplJni.get()
                .updateLocalTabId(mNativePtr, localGroupId, syncTabId, localTabId);
    }

    @Override
    public void setLocalObservationMode(boolean observeLocalChanges) {
        if (mIsObservingLocalChanges == observeLocalChanges) return;
        mIsObservingLocalChanges = observeLocalChanges;
        for (Observer observer : mObservers) {
            observer.onLocalObservationModeChanged(mIsObservingLocalChanges);
        }
    }

    @Override
    public boolean isObservingLocalChanges() {
        return mIsObservingLocalChanges;
    }

    @Override
    public boolean isRemoteDevice(@Nullable String syncCacheGuid) {
        if (mNativePtr == 0) return false;
        return TabGroupSyncServiceImplJni.get()
                .isRemoteDevice(mNativePtr, syncCacheGuid == null ? "" : syncCacheGuid);
    }

    @Override
    public boolean wasTabGroupClosedLocally(String syncTabGroupId) {
        if (mNativePtr == 0) return false;
        return TabGroupSyncServiceImplJni.get()
                .wasTabGroupClosedLocally(mNativePtr, syncTabGroupId);
    }

    @Override
    public void recordTabGroupEvent(EventDetails eventDetails) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get()
                .recordTabGroupEvent(
                        mNativePtr,
                        eventDetails.eventType,
                        eventDetails.localGroupId,
                        eventDetails.localTabId,
                        eventDetails.openingSource,
                        eventDetails.closingSource);
    }

    @Override
    public void updateArchivalStatus(String syncTabGroupId, boolean archivalStatus) {
        if (mNativePtr == 0) return;
        assert syncTabGroupId != null;
        TabGroupSyncServiceImplJni.get()
                .updateArchivalStatus(mNativePtr, syncTabGroupId, archivalStatus);
    }

    @Override
    public VersioningMessageController getVersioningMessageController() {
        return TabGroupSyncServiceImplJni.get().getVersioningMessageController(mNativePtr, this);
    }

    @Override
    public void setCollaborationAvailableInFinderForTesting(String collaborationId) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get()
                .setCollaborationAvailableInFinderForTesting(mNativePtr, collaborationId);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @CalledByNative
    private void onInitialized() {
        mInitialized = true;
        for (Observer observer : mObservers) {
            observer.onInitialized();
        }
    }

    @CalledByNative
    private void onTabGroupAdded(SavedTabGroup group, @TriggerSource int triggerSource) {
        for (Observer observer : mObservers) {
            observer.onTabGroupAdded(group, triggerSource);
        }
    }

    @CalledByNative
    private void onTabGroupUpdated(SavedTabGroup group, @TriggerSource int triggerSource) {
        for (Observer observer : mObservers) {
            observer.onTabGroupUpdated(group, triggerSource);
        }
    }

    @CalledByNative
    private void onTabGroupRemovedWithLocalId(
            LocalTabGroupId localId, @TriggerSource int triggerSource) {
        for (Observer observer : mObservers) {
            observer.onTabGroupRemoved(localId, triggerSource);
        }
    }

    @CalledByNative
    private void onTabGroupRemovedWithSyncId(String syncId, @TriggerSource int triggerSource) {
        for (Observer observer : mObservers) {
            observer.onTabGroupRemoved(syncId, triggerSource);
        }
    }

    @CalledByNative
    private void onTabGroupLocalIdChanged(String syncId, @Nullable LocalTabGroupId localId) {
        for (Observer observer : mObservers) {
            observer.onTabGroupLocalIdChanged(syncId, localId);
        }
    }

    @NativeMethods
    interface Natives {
        void addGroup(long nativeTabGroupSyncServiceAndroid, SavedTabGroup savedTabGroup);

        void removeGroupByLocalId(
                long nativeTabGroupSyncServiceAndroid, LocalTabGroupId localTabGroupId);

        void removeGroupBySyncId(long nativeTabGroupSyncServiceAndroid, String syncTabGroupId);

        void updateVisualData(
                long nativeTabGroupSyncServiceAndroid,
                LocalTabGroupId tabGroupId,
                String title,
                int color);

        void makeTabGroupShared(
                long nativeTabGroupSyncServiceAndroid,
                LocalTabGroupId tabGroupId,
                String collaborationId,
                @Nullable Callback<Boolean> tabGroupSharingCallback);

        void aboutToUnShareTabGroup(
                long nativeTabGroupSyncServiceAndroid,
                LocalTabGroupId tabGroupId,
                @Nullable Callback<Boolean> callback);

        void onTabGroupUnShareComplete(
                long nativeTabGroupSyncServiceAndroid, LocalTabGroupId tabGroupId, boolean success);

        void addTab(
                long nativeTabGroupSyncServiceAndroid,
                LocalTabGroupId groupId,
                int tabId,
                String title,
                GURL url,
                int position);

        void updateTab(
                long nativeTabGroupSyncServiceAndroid,
                LocalTabGroupId groupId,
                int tabId,
                String title,
                GURL url,
                int position);

        void removeTab(long nativeTabGroupSyncServiceAndroid, LocalTabGroupId groupId, int tabId);

        void moveTab(
                long nativeTabGroupSyncServiceAndroid,
                LocalTabGroupId groupId,
                int tabId,
                int newIndexInGroup);

        void setTabSelected(
                long nativeTabGroupSyncServiceAndroid,
                @Nullable LocalTabGroupId groupId,
                int tabId,
                String tabTitle);

        String[] getAllGroupIds(long nativeTabGroupSyncServiceAndroid);

        SavedTabGroup getGroupBySyncGroupId(
                long nativeTabGroupSyncServiceAndroid, String syncGroupId);

        SavedTabGroup getGroupByLocalGroupId(
                long nativeTabGroupSyncServiceAndroid, LocalTabGroupId localGroupId);

        Object[] getDeletedGroupIds(long nativeTabGroupSyncServiceAndroid);

        void updateLocalTabGroupMapping(
                long nativeTabGroupSyncServiceAndroid,
                String syncId,
                LocalTabGroupId localId,
                int openingSource);

        void removeLocalTabGroupMapping(
                long nativeTabGroupSyncServiceAndroid, LocalTabGroupId localId, int closingSource);

        void updateLocalTabId(
                long nativeTabGroupSyncServiceAndroid,
                LocalTabGroupId localGroupId,
                String syncTabId,
                int localTabId);

        boolean isRemoteDevice(long nativeTabGroupSyncServiceAndroid, String syncCacheGuid);

        boolean wasTabGroupClosedLocally(
                long nativeTabGroupSyncServiceAndroid, String syncTabGroupId);

        void recordTabGroupEvent(
                long nativeTabGroupSyncServiceAndroid,
                int eventType,
                @Nullable LocalTabGroupId localGroupId,
                int localTabId,
                int openingSource,
                int closingSource);

        void updateArchivalStatus(
                long nativeTabGroupSyncServiceAndroid,
                String syncTabGroupId,
                boolean archivalStatus);

        VersioningMessageController getVersioningMessageController(
                long nativeTabGroupSyncServiceAndroid, TabGroupSyncServiceImpl caller);

        void setCollaborationAvailableInFinderForTesting(
                long nativeTabGroupSyncServiceAndroid, String collaborationId);
    }
}
