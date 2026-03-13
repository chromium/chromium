// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** JNI bridge to the native {@code SidePanelUIAndroid}. */
@NullMarked
public final class SidePanelUIAndroidBridge {

    /** Address of the native {@code SidePanelUIAndroid}. */
    private long mNativeSidePanelUIAndroid;

    public SidePanelUIAndroidBridge() {}

    /** Creates the native {@code SidePanelUIAndroid}. */
    public void createNativePtr() {
        assert mNativeSidePanelUIAndroid == 0 : "Native SidePanelUIAndroid already exists";
        mNativeSidePanelUIAndroid = SidePanelUIAndroidBridgeJni.get().create(this);
    }

    /** Destroys all objects owned by this class. */
    public void destroy() {
        if (mNativeSidePanelUIAndroid != 0) {
            SidePanelUIAndroidBridgeJni.get().destroy(mNativeSidePanelUIAndroid);
        }
    }

    long getNativePtrForTesting() {
        return mNativeSidePanelUIAndroid;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeSidePanelUIAndroid = 0;
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code SidePanelUIAndroid}.
         *
         * @param caller The Java object calling this method.
         * @return The address of the native {@code SidePanelUIAndroid}.
         */
        long create(SidePanelUIAndroidBridge caller);

        /**
         * Destroys the native {@code SidePanelUIAndroid}.
         *
         * @param nativeSidePanelUIAndroid The address of the native {@code SidePanelUIAndroid}.
         */
        void destroy(long nativeSidePanelUIAndroid);
    }
}
