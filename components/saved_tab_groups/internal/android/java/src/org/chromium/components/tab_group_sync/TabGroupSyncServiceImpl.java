// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Java side of the JNI bridge between TabGroupSyncServiceImpl in Java and C++. All method calls are
 * delegated to the native C++ class.
 */
@JNINamespace("tab_groups")
public class TabGroupSyncServiceImpl implements TabGroupSyncService {
    private final ObserverList<TabGroupSyncService.Observer> mObservers = new ObserverList<>();
    private long mNativePtr;
    private boolean mInitialized;

    @CalledByNative
    private static TabGroupSyncServiceImpl create(long nativePtr) {
        return new TabGroupSyncServiceImpl(nativePtr);
    }

    private TabGroupSyncServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
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
    public String createGroup(LocalTabGroupId groupId) {
        if (mNativePtr == 0) return null;
        assert groupId != null;
        return TabGroupSyncServiceImplJni.get().createGroup(mNativePtr, this, groupId);
    }

    @Override
    public void removeGroup(LocalTabGroupId localTabGroupId) {
        if (mNativePtr == 0) return;
        assert localTabGroupId != null;
        TabGroupSyncServiceImplJni.get().removeGroupByLocalId(mNativePtr, this, localTabGroupId);
    }

    @Override
    public void removeGroup(String syncTabGroupId) {
        if (mNativePtr == 0) return;
        assert syncTabGroupId != null;
        TabGroupSyncServiceImplJni.get().removeGroupBySyncId(mNativePtr, this, syncTabGroupId);
    }

    @Override
    public void updateVisualData(LocalTabGroupId groupId, String title, int color) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get().updateVisualData(mNativePtr, this, groupId, title, color);
    }

    @Override
    public void makeTabGroupShared(LocalTabGroupId tabGroupId, String collaborationId) {
        if (mNativePtr == 0) return;
        assert tabGroupId != null;
        TabGroupSyncServiceImplJni.get()
                .makeTabGroupShared(mNativePtr, this, tabGroupId, collaborationId);
    }

    @Override
    public void addTab(LocalTabGroupId groupId, int tabId, String title, GURL url, int position) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get()
                .addTab(mNativePtr, this, groupId, tabId, title, url, position);
    }

    @Override
    public void updateTab(
            LocalTabGroupId groupId, int tabId, String title, GURL url, int position) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get()
                .updateTab(mNativePtr, this, groupId, tabId, title, url, position);
    }

    @Override
    public void removeTab(LocalTabGroupId groupId, int tabId) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get().removeTab(mNativePtr, this, groupId, tabId);
    }

    @Override
    public void moveTab(LocalTabGroupId groupId, int tabId, int newIndexInGroup) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get().moveTab(mNativePtr, this, groupId, tabId, newIndexInGroup);
    }

    @Override
    public void onTabSelected(LocalTabGroupId groupId, int tabId) {
        if (mNativePtr == 0) return;
        assert groupId != null;
        TabGroupSyncServiceImplJni.get().onTabSelected(mNativePtr, this, groupId, tabId);
    }

    @Override
    public String[] getAllGroupIds() {
        if (mNativePtr == 0) return new String[0];
        return TabGroupSyncServiceImplJni.get().getAllGroupIds(mNativePtr, this);
    }

    @Override
    public SavedTabGroup getGroup(String syncGroupId) {
        if (mNativePtr == 0) return null;
        return TabGroupSyncServiceImplJni.get()
                .getGroupBySyncGroupId(mNativePtr, this, syncGroupId);
    }

    @Override
    public SavedTabGroup getGroup(LocalTabGroupId localGroupId) {
        assert localGroupId != null;
        return TabGroupSyncServiceImplJni.get()
                .getGroupByLocalGroupId(mNativePtr, this, localGroupId);
    }

    @Override
    public void updateLocalTabGroupMapping(
            String syncId, LocalTabGroupId localId, @OpeningSource int openingSource) {
        if (mNativePtr == 0) return;
        assert localId != null;
        TabGroupSyncServiceImplJni.get()
                .updateLocalTabGroupMapping(mNativePtr, this, syncId, localId, openingSource);
    }

    @Override
    public void removeLocalTabGroupMapping(
            LocalTabGroupId localId, @ClosingSource int closingSource) {
        if (mNativePtr == 0) return;
        assert localId != null;
        TabGroupSyncServiceImplJni.get()
                .removeLocalTabGroupMapping(mNativePtr, this, localId, closingSource);
    }

    @Override
    public List<LocalTabGroupId> getDeletedGroupIds() {
        if (mNativePtr == 0) return new ArrayList<>();
        List<LocalTabGroupId> deletedIds = new ArrayList<>();
        Object[] objects = TabGroupSyncServiceImplJni.get().getDeletedGroupIds(mNativePtr, this);
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
                .updateLocalTabId(mNativePtr, this, localGroupId, syncTabId, localTabId);
    }

    @Override
    public boolean isRemoteDevice(String syncCacheGuid) {
        if (mNativePtr == 0) return false;
        return TabGroupSyncServiceImplJni.get()
                .isRemoteDevice(
                        mNativePtr, this, syncCacheGuid == null ? new String() : syncCacheGuid);
    }

    @Override
    public void recordTabGroupEvent(EventDetails eventDetails) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get()
                .recordTabGroupEvent(
                        mNativePtr,
                        this,
                        eventDetails.eventType,
                        eventDetails.localGroupId,
                        eventDetails.localTabId,
                        eventDetails.openingSource,
                        eventDetails.closingSource);
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
        String createGroup(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId groupId);

        void removeGroupByLocalId(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId localTabGroupId);

        void removeGroupBySyncId(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                String syncTabGroupId);

        void updateVisualData(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId tabGroupId,
                String title,
                int color);

        void makeTabGroupShared(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId tabGroupId,
                String collaborationId);

        void addTab(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId groupId,
                int tabId,
                String title,
                GURL url,
                int position);

        void updateTab(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId groupId,
                int tabId,
                String title,
                GURL url,
                int position);

        void removeTab(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId groupId,
                int tabId);

        void moveTab(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId groupId,
                int tabId,
                int newIndexInGroup);

        void onTabSelected(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId groupId,
                int tabId);

        String[] getAllGroupIds(
                long nativeTabGroupSyncServiceAndroid, TabGroupSyncServiceImpl caller);

        SavedTabGroup getGroupBySyncGroupId(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                String syncGroupId);

        SavedTabGroup getGroupByLocalGroupId(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId localGroupId);

        Object[] getDeletedGroupIds(
                long nativeTabGroupSyncServiceAndroid, TabGroupSyncServiceImpl caller);

        void updateLocalTabGroupMapping(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                String syncId,
                LocalTabGroupId localId,
                int openingSource);

        void removeLocalTabGroupMapping(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId localId,
                int closingSource);

        void updateLocalTabId(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                LocalTabGroupId localGroupId,
                String syncTabId,
                int localTabId);

        boolean isRemoteDevice(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                String syncCacheGuid);

        void recordTabGroupEvent(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                int eventType,
                LocalTabGroupId localGroupId,
                int localTabId,
                int openingSource,
                int closingSource);
    }
}
