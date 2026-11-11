// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test_support;

import android.content.ClipData;
import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.dragdrop.DragAndDropDelegateImpl;
import org.chromium.ui.dragdrop.DropDataProviderUtils;

/**
 * Test-only {@link DragAndDropDelegateImpl.DragStartInterceptorForTesting} backing the native
 * {@code drag_and_drop_test_utils::DragStartWaiter} on Android.
 *
 * <p>It observes drags that the production {@link DragAndDropDelegateImpl} of a single container
 * view starts, reporting the {@link ClipData} built for the drag (for an image, with its bytes
 * cached in {@code DropDataProvider} exactly as in a real drag) to native. When suppressed, it
 * consumes the drag so that no system drag starts.
 */
@JNINamespace("drag_and_drop_test_utils")
@NullMarked
public class DragStartWaiterBridge
        implements DragAndDropDelegateImpl.DragStartInterceptorForTesting {
    private long mNativeDragStartWaiterAndroid;
    private final View mContainerView;
    private boolean mSuppressSystemDrag;

    private DragStartWaiterBridge(long nativeDragStartWaiterAndroid, View containerView) {
        mNativeDragStartWaiterAndroid = nativeDragStartWaiterAndroid;
        mContainerView = containerView;
    }

    /**
     * @param containerView The container view of the WebContents whose drags are observed. Drags
     *     started from other views are left untouched.
     */
    @CalledByNative
    private static DragStartWaiterBridge create(
            long nativeDragStartWaiterAndroid, View containerView) {
        DragStartWaiterBridge bridge =
                new DragStartWaiterBridge(nativeDragStartWaiterAndroid, containerView);
        DragAndDropDelegateImpl.setDragStartInterceptorForTesting(bridge);
        return bridge;
    }

    @CalledByNative
    private void setSuppressSystemDrag(boolean suppress) {
        mSuppressSystemDrag = suppress;
    }

    // Mirrors DragAndDropDelegateImpl#onDragEnd(), clearing any image cached in DropDataProvider
    // when a suppressed drag ends without receiving a system ACTION_DRAG_ENDED event.
    @CalledByNative
    private void endSuppressedDrag() {
        // May fail if the provider is not declared in test APKs that do not exercise image drags.
        DropDataProviderUtils.clearImageCache(/* imageIsInUse= */ false);
    }

    @CalledByNative
    private void destroy() {
        mNativeDragStartWaiterAndroid = 0;
        DragAndDropDelegateImpl.setDragStartInterceptorForTesting(null);
    }

    @Override
    public boolean onDragStarting(View containerView, @Nullable ClipData clipData) {
        if (containerView != mContainerView || !mSuppressSystemDrag) {
            return false;
        }
        notifyDragStarted(clipData);
        return true;
    }

    @Override
    public void onDragStarted(View containerView, @Nullable ClipData clipData) {
        if (containerView == mContainerView) {
            notifyDragStarted(clipData);
        }
    }

    private void notifyDragStarted(@Nullable ClipData clipData) {
        if (mNativeDragStartWaiterAndroid != 0) {
            DragStartWaiterBridgeJni.get().onDragStarted(mNativeDragStartWaiterAndroid, clipData);
        }
    }

    @NativeMethods
    interface Natives {
        void onDragStarted(long nativeDragStartWaiterAndroid, @Nullable ClipData clipData);
    }
}
