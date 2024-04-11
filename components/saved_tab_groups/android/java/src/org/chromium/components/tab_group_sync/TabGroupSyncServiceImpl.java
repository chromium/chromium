// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;

/**
 * Java side of the JNI bridge between TabGroupSyncServiceImpl in Java and C++. All method calls are
 * delegated to the native C++ class.
 */
@JNINamespace("tab_groups")
public class TabGroupSyncServiceImpl implements TabGroupSyncService {
    private final ObserverList<TabGroupSyncService.Observer> mObservers = new ObserverList<>();
    private long mNativePtr;

    @CalledByNative
    private static TabGroupSyncServiceImpl create(long nativePtr) {
        return new TabGroupSyncServiceImpl(nativePtr);
    }

    private TabGroupSyncServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public void removeGroup(int groupId) {
        if (mNativePtr == 0) return;
        TabGroupSyncServiceImplJni.get().removeGroup(mNativePtr, this, groupId);
    }

    @Override
    public void addObserver(TabGroupSyncService.Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabGroupSyncService.Observer observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @CalledByNative
    private void onInitialized() {}

    @CalledByNative
    private void onTabGroupAdded(SavedTabGroup group) {}

    @CalledByNative
    private void onTabGroupUpdated(SavedTabGroup group) {}

    @CalledByNative
    private void onTabGroupRemoved(int localId) {}

    @NativeMethods
    interface Natives {
        void removeGroup(
                long nativeTabGroupSyncServiceAndroid, TabGroupSyncServiceImpl caller, int groupId);
    }
}
