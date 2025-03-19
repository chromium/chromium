// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Java side of the JNI bridge between GroupSuggestionsServiceImpl in Java and C++. All method calls
 * are delegated to the native C++ class.
 */
@JNINamespace("visited_url_ranking")
public class GroupSuggestionsServiceImpl implements GroupSuggestionsService {
    private long mNativePtr;

    private final DelegateBridge mDelegateBridge = new DelegateBridge();

    @CalledByNative
    private static GroupSuggestionsServiceImpl create(long nativePtr) {
        return new GroupSuggestionsServiceImpl(nativePtr);
    }

    private GroupSuggestionsServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @CalledByNative
    private DelegateBridge getDelegateBridge() {
        return mDelegateBridge;
    }

    @Override
    public void didAddTab(int tabId, int tabLaunchType) {
        GroupSuggestionsServiceImplJni.get().didAddTab(mNativePtr, tabId, tabLaunchType);
    }

    @Override
    public void didSelectTab(int tabId, int tabSelectionType, int lastId) {
        GroupSuggestionsServiceImplJni.get()
                .didSelectTab(mNativePtr, tabId, tabSelectionType, lastId);
    }

    @Override
    public void didEnterTabSwitcher() {
        GroupSuggestionsServiceImplJni.get().didEnterTabSwitcher(mNativePtr);
    }

    @Override
    // TODO(crbug.com/397221723): Support registering delegate for specific window.
    public void registerDelegate(GroupSuggestionsService.Delegate delegate, int windowId) {
        mDelegateBridge.registerDelegate(delegate);
    }

    @Override
    public void unregisterDelegate(GroupSuggestionsService.Delegate delegate) {
        mDelegateBridge.unregisterDelegate(delegate);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        void didAddTab(long nativeGroupSuggestionsServiceAndroid, int tabId, int tabLaunchType);

        void didSelectTab(
                long nativeGroupSuggestionsServiceAndroid,
                int tabId,
                int tabSelectionType,
                int lastId);

        void didEnterTabSwitcher(long nativeGroupSuggestionsServiceAndroid);
    }
}
