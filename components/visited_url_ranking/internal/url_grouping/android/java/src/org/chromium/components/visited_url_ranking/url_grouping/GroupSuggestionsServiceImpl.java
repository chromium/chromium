// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.tab.TabLaunchType;

/**
 * Java side of the JNI bridge between GroupSuggestionsServiceImpl in Java and C++. All method calls
 * are delegated to the native C++ class.
 */
@JNINamespace("visited_url_ranking")
public class GroupSuggestionsServiceImpl implements GroupSuggestionsService {
    private long mNativePtr;

    @CalledByNative
    private static GroupSuggestionsServiceImpl create(long nativePtr) {
        return new GroupSuggestionsServiceImpl(nativePtr);
    }

    private GroupSuggestionsServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public void didAddTab(int tabId, @TabLaunchType int type) {
        GroupSuggestionsServiceImplJni.get().didAddTab(mNativePtr, tabId, type);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        void didAddTab(long nativeGroupSuggestionsServiceAndroid, int tabId, int type);
    }
}
