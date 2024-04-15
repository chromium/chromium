// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.url.GURL;

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
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> observer.onInitialized());
        }
    }

    @Override
    public void removeObserver(TabGroupSyncService.Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public String createGroup(int groupId) {
        if (mNativePtr == 0) return null;
        return TabGroupSyncServiceImplJni.get().createGroup(mNativePtr, this, groupId);
    }

    @Override
    public void removeGroup(int groupId) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get().removeGroup(mNativePtr, this, groupId);
    }

    @Override
    public void updateVisualData(int tabGroupId, String title, int color) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get()
                .updateVisualData(mNativePtr, this, tabGroupId, title, color);
    }

    @Override
    public void addTab(int groupId, int tabId, String title, GURL url, int position) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get()
                .addTab(mNativePtr, this, groupId, tabId, title, url, position);
    }

    @Override
    public void updateTab(int groupId, int tabId, String title, GURL url, int position) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get()
                .updateTab(mNativePtr, this, groupId, tabId, title, url, position);
    }

    @Override
    public void removeTab(int groupId, int tabId) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get().removeTab(mNativePtr, this, groupId, tabId);
    }

    @Override
    public String[] getAllGroupIds() {
        if (mNativePtr == 0) return null;
        return TabGroupSyncServiceImplJni.get().getAllGroupIds(mNativePtr, this);
    }

    @Override
    public SavedTabGroup getGroup(String syncGroupId) {
        if (mNativePtr == 0) return null;
        return TabGroupSyncServiceImplJni.get()
                .getGroupBySyncGroupId(mNativePtr, this, syncGroupId);
    }

    @Override
    public SavedTabGroup getGroup(int localGroupId) {
        if (mNativePtr == 0) return null;
        return TabGroupSyncServiceImplJni.get()
                .getGroupByLocalGroupId(mNativePtr, this, localGroupId);
    }

    @Override
    public void updateLocalTabGroupId(String syncId, int localId) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get().updateLocalTabGroupId(mNativePtr, this, syncId, localId);
    }

    @Override
    public void updateLocalTabId(int localGroupId, String syncTabId, int localTabId) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get()
                .updateLocalTabId(mNativePtr, this, localGroupId, syncTabId, localTabId);
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
    private void onTabGroupAdded(SavedTabGroup group) {
        for (Observer observer : mObservers) {
            observer.onTabGroupAdded(group);
        }
    }

    @CalledByNative
    private void onTabGroupUpdated(SavedTabGroup group) {
        for (Observer observer : mObservers) {
            observer.onTabGroupUpdated(group);
        }
    }

    @CalledByNative
    private void onTabGroupRemovedWithLocalId(int localId) {
        for (Observer observer : mObservers) {
            observer.onTabGroupRemoved(localId);
        }
    }

    @CalledByNative
    private void onTabGroupRemovedWithSyncId(String syncId) {
        for (Observer observer : mObservers) {
            observer.onTabGroupRemoved(syncId);
        }
    }

    @NativeMethods
    interface Natives {
        String createGroup(
                long nativeTabGroupSyncServiceAndroid, TabGroupSyncServiceImpl caller, int groupId);

        void removeGroup(
                long nativeTabGroupSyncServiceAndroid, TabGroupSyncServiceImpl caller, int groupId);

        void updateVisualData(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                int tabGroupId,
                String title,
                int color);

        void addTab(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                int groupId,
                int tabId,
                String title,
                GURL url,
                int position);

        void updateTab(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                int groupId,
                int tabId,
                String title,
                GURL url,
                int position);

        void removeTab(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                int groupId,
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
                int localGroupId);

        void updateLocalTabGroupId(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                String syncId,
                int localId);

        void updateLocalTabId(
                long nativeTabGroupSyncServiceAndroid,
                TabGroupSyncServiceImpl caller,
                int localGroupId,
                String syncTabId,
                int localTabId);
    }
}
