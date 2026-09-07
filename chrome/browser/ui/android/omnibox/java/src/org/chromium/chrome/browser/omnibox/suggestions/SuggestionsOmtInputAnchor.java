// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.os.Build;
import android.view.SurfaceControl;
import android.view.WindowManager;
import android.window.InputTransferToken;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

/**
 * Anchor for Off-Main-Thread (OMT) Suggestions Input routing JNI connection.
 *
 * <p>Binds the Java-side SuggestionsOmtOverlayView lifecycle with native AInputReceiver
 * registration. Intercepting ACTION_DOWN on a background thread allows measuring hardware
 * touch-down delay off-main-thread before transferring touch gesture focus back to the main window
 * token via WindowManager.transferTouchGesture().
 */
@NullMarked
@JNINamespace("omnibox")
public class SuggestionsOmtInputAnchor {

    /**
     * Transfers active touch gesture from the overlay surface token to the main window token.
     *
     * @param fromToken Input transfer token of the source overlay surface.
     * @param toToken Input transfer token of the destination main Chrome window.
     * @return Whether the touch gesture transfer request succeeded.
     */
    @CalledByNative
    public static boolean transferTouch(InputTransferToken fromToken, InputTransferToken toToken) {
        // Gated to Android 16+ (Baklava / API Level 36+) to avoid the AInputReceiver_release()
        // crash bug present in Android 15 (https://crbug.com/368251173).
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
            WindowManager windowManager =
                    ContextUtils.getApplicationContext().getSystemService(WindowManager.class);
            if (windowManager != null) {
                boolean result = windowManager.transferTouchGesture(fromToken, toToken);
                RecordHistogram.recordBooleanHistogram(
                        "Android.Omnibox.OMTPrefetch.TouchTransferSuccess", result);
                return result;
            }
        }
        return false;
    }

    @NativeMethods
    public interface Natives {
        /**
         * Registers a native AInputReceiver on the background thread's ALooper.
         *
         * @param surfaceControl Native surface control backing the overlay SurfaceView.
         * @param mainWindowToken Main window input transfer token to pair with the receiver.
         * @return Memory address of the allocated native OmtInputReceiverState instance.
         */
        long registerInputReceiver(
                SurfaceControl surfaceControl, InputTransferToken mainWindowToken);

        /**
         * Releases the native AInputReceiver and frees the associated receiver state.
         *
         * @param stateAddress Memory address of the native OmtInputReceiverState to release.
         */
        void unregisterInputReceiver(long stateAddress);
    }
}
