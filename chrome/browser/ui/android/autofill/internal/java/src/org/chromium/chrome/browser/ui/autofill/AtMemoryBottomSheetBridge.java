// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** JNI wrapper for the @memory bottom sheet. */
@NullMarked
@JNINamespace("autofill")
public class AtMemoryBottomSheetBridge implements AtMemoryBottomSheetCoordinator.Delegate {
    private long mNativeAtMemoryBottomSheetBridge;
    private final AtMemoryBottomSheetCoordinator mCoordinator;
    private boolean mInitialized;

    @CalledByNative
    public AtMemoryBottomSheetBridge(
            long nativeAtMemoryBottomSheetBridge, WindowAndroid windowAndroid) {
        mNativeAtMemoryBottomSheetBridge = nativeAtMemoryBottomSheetBridge;
        mCoordinator = new AtMemoryBottomSheetCoordinator();

        Context context = windowAndroid.getContext().get();
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (context == null || bottomSheetController == null) return;

        mInitialized = true;
        mCoordinator.initialize(context, bottomSheetController, this);
    }

    @CalledByNative
    public void show() {
        if (!mInitialized) {
            onDismissed();
            return;
        }
        mCoordinator.show();
    }

    @CalledByNative
    public void destroy() {
        mNativeAtMemoryBottomSheetBridge = 0;
        mCoordinator.destroy();
    }

    @Override
    public void onDismissed() {
        if (mNativeAtMemoryBottomSheetBridge != 0) {
            AtMemoryBottomSheetBridgeJni.get().onDismissed(mNativeAtMemoryBottomSheetBridge);
        }
    }

    @NativeMethods
    public interface Natives {
        void onDismissed(long nativeAtMemoryBottomSheetBridge);
    }
}
