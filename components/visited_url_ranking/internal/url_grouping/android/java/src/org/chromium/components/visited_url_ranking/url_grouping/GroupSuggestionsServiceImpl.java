// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

/**
 * Java side of the JNI bridge between GroupSuggestionsServiceImpl in Java and C++. All method calls
 * are delegated to the native C++ class.
 */
@JNINamespace("visited_url_ranking")
@NullMarked
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
    public void didSelectTab(int tabId, GURL url, int tabSelectionType, int lastId) {
        GroupSuggestionsServiceImplJni.get()
                .didSelectTab(mNativePtr, tabId, url, tabSelectionType, lastId);
    }

    @Override
    public void willCloseTab(int tabId) {
        GroupSuggestionsServiceImplJni.get().willCloseTab(mNativePtr, tabId);
    }

    @Override
    public void tabClosureUndone(int tabId) {
        GroupSuggestionsServiceImplJni.get().tabClosureUndone(mNativePtr, tabId);
    }

    @Override
    public void tabClosureCommitted(int tabId) {
        GroupSuggestionsServiceImplJni.get().tabClosureCommitted(mNativePtr, tabId);
    }

    @Override
    public void onDidFinishNavigation(int tabId, int transitionType) {
        GroupSuggestionsServiceImplJni.get()
                .onDidFinishNavigation(mNativePtr, tabId, transitionType);
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

    @Override
    public @Nullable CachedSuggestions getCachedSuggestions(int windowId) {
        if (mNativePtr == 0) {
            // Return CachedSuggestions with an empty list if the native service isn't initialized.
            return new CachedSuggestions(null, emptyJniCallback());
        }
        return GroupSuggestionsServiceImplJni.get().getCachedSuggestions(mNativePtr, windowId);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    private static JniOnceCallback<UserResponseMetadata> emptyJniCallback() {
        return new JniOnceCallback<>() {
            @Override
            public void destroy() {}

            @Override
            public void onResult(UserResponseMetadata result) {}

            @Override
            public Runnable bind(UserResponseMetadata result) {
                return () -> {};
            }
        };
    }

    @NativeMethods
    interface Natives {
        void didAddTab(long nativeGroupSuggestionsServiceAndroid, int tabId, int tabLaunchType);

        void didSelectTab(
                long nativeGroupSuggestionsServiceAndroid,
                int tabId,
                GURL url,
                @TabSelectionType int tabSelectionType,
                int lastId);

        void willCloseTab(long nativeGroupSuggestionsServiceAndroid, int tabId);

        void tabClosureUndone(long nativeGroupSuggestionsServiceAndroid, int tabId);

        void tabClosureCommitted(long nativeGroupSuggestionsServiceAndroid, int tabId);

        void onDidFinishNavigation(
                long nativeGroupSuggestionsServiceAndroid, int tabId, int transitionType);

        void didEnterTabSwitcher(long nativeGroupSuggestionsServiceAndroid);

        CachedSuggestions getCachedSuggestions(
                long nativeGroupSuggestionsServiceAndroid, int windowId);
    }
}
