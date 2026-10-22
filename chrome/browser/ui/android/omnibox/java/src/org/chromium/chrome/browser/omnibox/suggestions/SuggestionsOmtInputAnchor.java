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
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

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
    @JniType("::omnibox::OmtInputReceiverState")
    interface NativeOmtInputReceiverState extends JniTypeToken {}

    /**
     * Transfers active touch gesture from the overlay surface token to the main window token.
     *
     * <p>Invoked from native code on the background input thread upon intercepting ACTION_DOWN.
     *
     * @param fromToken Input transfer token of the source overlay surface.
     * @param toToken Input transfer token of the destination main Chrome window.
     * @param onTouchTransferred Callback invoked when the touch gesture transfer succeeds.
     * @return Whether the touch gesture transfer request succeeded.
     */
    @CalledByNative
    static boolean transferTouch(
            InputTransferToken fromToken, InputTransferToken toToken, Runnable onTouchTransferred) {
        // Gated to Android 16+ (Baklava / API Level 36+) to avoid the AInputReceiver_release()
        // crash bug present in Android 15 (https://crbug.com/368251173).
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
            WindowManager windowManager =
                    ContextUtils.getApplicationContext().getSystemService(WindowManager.class);
            if (windowManager != null) {
                boolean result = windowManager.transferTouchGesture(fromToken, toToken);
                RecordHistogram.recordBooleanHistogram(
                        "Android.Omnibox.OMTPrefetch.TouchTransferSuccess", result);
                if (result) {
                    onTouchTransferred.run();
                }
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
         * <p>Must be invoked on the background HandlerThread where the ALooper is active.
         *
         * @param surfaceControl Native surface control backing the overlay SurfaceView.
         * @param mainWindowToken Main window input transfer token to pair with the receiver.
         * @param onTouchTransferred Callback invoked when a touch gesture is transferred from the
         *     overlay surface to the main window.
         * @return Owned handle to the allocated native OmtInputReceiverState instance, or null if
         *     registration failed. Must be destroyed on the same background HandlerThread.
         */
        @Nullable JniUniquePtr<NativeOmtInputReceiverState> registerInputReceiver(
                SurfaceControl surfaceControl,
                InputTransferToken mainWindowToken,
                Runnable onTouchTransferred);
    }
}
