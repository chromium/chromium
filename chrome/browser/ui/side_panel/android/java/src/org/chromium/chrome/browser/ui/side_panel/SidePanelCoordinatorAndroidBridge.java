// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** JNI bridge to the native {@code SidePanelCoordinatorAndroid}. */
@NullMarked
public final class SidePanelCoordinatorAndroidBridge {

    /** Address of the native {@code SidePanelCoordinatorAndroid}. */
    private long mNativeSidePanelCoordinatorAndroid;

    public SidePanelCoordinatorAndroidBridge() {}

    /** Creates the native {@code SidePanelCoordinatorAndroid}. */
    public void createNativePtr() {
        assert mNativeSidePanelCoordinatorAndroid == 0
                : "Native SidePanelCoordinatorAndroid already exists";
        mNativeSidePanelCoordinatorAndroid =
                SidePanelCoordinatorAndroidBridgeJni.get().create(this);
    }

    /** Destroys all objects owned by this class. */
    public void destroy() {
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidBridgeJni.get().destroy(mNativeSidePanelCoordinatorAndroid);
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
         * @return The address of the native {@code SidePanelCoordinatorAndroid}.
         */
        long create(SidePanelCoordinatorAndroidBridge caller);

        /**
         * Destroys the native {@code SidePanelCoordinatorAndroid}.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void destroy(long nativeSidePanelCoordinatorAndroid);
    }
}
