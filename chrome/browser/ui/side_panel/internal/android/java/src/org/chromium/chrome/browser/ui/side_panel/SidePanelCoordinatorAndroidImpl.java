// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Implements {@code SidePanelCoordinatorAndroid}. */
@NullMarked
public final class SidePanelCoordinatorAndroidImpl implements SidePanelCoordinatorAndroid {

    /** Address of the native {@code SidePanelCoordinatorAndroid}. */
    private long mNativeSidePanelCoordinatorAndroid;

    public SidePanelCoordinatorAndroidImpl() {}

    @Override
    public void onAddedToTask(long nativeBrowserWindowPtr) {
        createNativePtr(nativeBrowserWindowPtr);
    }

    @Override
    public void onFeatureRemoved() {
        destroyNativePtr();
    }

    @VisibleForTesting
    void createNativePtr(long nativeBrowserWindowPtr) {
        assert nativeBrowserWindowPtr != 0
                : "Native BrowserWindowInterface pointer shouldn't be null. Is the"
                        + " ChromeAndroidTaskFeatureKey correct?";
        assert mNativeSidePanelCoordinatorAndroid == 0
                : "Native SidePanelCoordinatorAndroid already exists";
        mNativeSidePanelCoordinatorAndroid =
                SidePanelCoordinatorAndroidImplJni.get().create(this, nativeBrowserWindowPtr);
    }

    @VisibleForTesting
    void destroyNativePtr() {
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get().destroy(mNativeSidePanelCoordinatorAndroid);
        }
    }

    long getNativePtrForTesting() {
        return mNativeSidePanelCoordinatorAndroid;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeSidePanelCoordinatorAndroid = 0;
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code SidePanelCoordinatorAndroid}.
         *
         * @param caller The Java object calling this method.
         * @param nativeBrowserWindowPtr The pointer to the native {@code BrowserWindowInterface}.
         * @return The address of the native {@code SidePanelCoordinatorAndroid}.
         */
        long create(SidePanelCoordinatorAndroidImpl caller, long nativeBrowserWindowPtr);

        /**
         * Destroys the native {@code SidePanelCoordinatorAndroid}.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void destroy(long nativeSidePanelCoordinatorAndroid);
    }
}
