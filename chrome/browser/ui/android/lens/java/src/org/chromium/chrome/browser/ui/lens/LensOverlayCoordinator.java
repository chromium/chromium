// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.lens;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** Coordinator for the Lens Overlay feature. */
@JNINamespace("lens")
@NullMarked
public class LensOverlayCoordinator {
    private static final String TAG = "LensOverlay";
    private long mNativeLensOverlayControllerAndroid;

    public LensOverlayCoordinator(WebContents webContents) {
        mNativeLensOverlayControllerAndroid =
                LensOverlayCoordinatorJni.get().init(this, webContents);
    }

    /** Starts the Lens Overlay. */
    public void start(@LensOverlayInvocationSource int invocationSource) {
        Log.i(TAG, "Starting Lens Overlay");
        LensOverlayCoordinatorJni.get()
                .showUI(mNativeLensOverlayControllerAndroid, invocationSource);

        // TODO(b/493627069): Remove this auto-destroy once the full overlay UI is implemented
        // and the lifecycle is properly managed by the tab/WebContents.
        destroy();
    }

    public void destroy() {
        if (mNativeLensOverlayControllerAndroid != 0) {
            LensOverlayCoordinatorJni.get().destroy(mNativeLensOverlayControllerAndroid);
            mNativeLensOverlayControllerAndroid = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(
                LensOverlayCoordinator caller,
                @JniType("content::WebContents*") WebContents webContents);

        void showUI(
                long nativeLensOverlayControllerAndroid,
                @LensOverlayInvocationSource int invocationSource);

        void destroy(long nativeLensOverlayControllerAndroid);
    }
}
