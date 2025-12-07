// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;

/**
 * Test delegate that registers itself to the Java service, and sends notification to the native
 * test.
 *
 * <p>See group_suggestions_service_android_unittest.cc for usage.
 */
@JNINamespace("visited_url_ranking")
public class TestServiceDelegate implements GroupSuggestionsService.Delegate {
    private final long mNativePtr;
    private int mShowSuggestionCount;
    private int mOnDumpStateForFeedbackCount;

    public TestServiceDelegate(long nativePtr) {
        this.mNativePtr = nativePtr;
        this.mShowSuggestionCount = 0;
        this.mOnDumpStateForFeedbackCount = 0;
    }

    @CalledByNative
    public static TestServiceDelegate createAndAdd(
            GroupSuggestionsService service, long nativePtr) {
        TestServiceDelegate delegate = new TestServiceDelegate(nativePtr);
        service.registerDelegate(delegate, 1);
        return delegate;
    }

    @CalledByNative
    public void destroy(GroupSuggestionsService service) {
        service.unregisterDelegate(this);
    }

    @CalledByNative
    private int getShowSuggestionCount() {
        return mShowSuggestionCount;
    }

    @CalledByNative
    private int getOnDumpStateForFeedbackCount() {
        return mOnDumpStateForFeedbackCount;
    }

    @Override
    public void showSuggestion(
            GroupSuggestions groupSuggestions, Callback<UserResponseMetadata> callback) {
        mShowSuggestionCount++;
        TestServiceDelegateJni.get().onDelegateNotify(mNativePtr);
    }

    @Override
    public void onDumpStateForFeedback(String dumpState) {
        mOnDumpStateForFeedbackCount++;
        TestServiceDelegateJni.get().onDelegateNotify(mNativePtr);
    }

    @NativeMethods
    interface Natives {
        void onDelegateNotify(long delegatePtr);
    }
}
