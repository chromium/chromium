// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.JniOnceCallback;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.mojom.WindowShowState;

import java.util.Locale;

/** Java class that will be used by the native {@code #CreateBrowserWindow()} function. */
@NullMarked
final class BrowserWindowCreatorBridge {

    private static final String TAG = "BwsrWndwCreatrBrdge";

    private BrowserWindowCreatorBridge() {}

    @CalledByNative
    @VisibleForTesting
    static long createBrowserWindow(AndroidBrowserWindowCreateParams createParams) {
        if (!canCreateBrowserWindow(createParams)) {
            return 0L;
        }

        ChromeAndroidTask task =
                ChromeAndroidTaskTrackerImpl.getInstance().createPendingTask(createParams, null);
        return task == null
                ? 0L
                : task.getOrCreateNativeBrowserWindowPtr(createParams.getProfile());
    }

    @CalledByNative
    @VisibleForTesting
    static void createBrowserWindowAsync(
            AndroidBrowserWindowCreateParams createParams, JniOnceCallback<Long> callback) {
        if (!canCreateBrowserWindow(createParams)) {
            callback.onResult(0L);
            return;
        }

        ChromeAndroidTaskTrackerImpl.getInstance().createPendingTask(createParams, callback);
    }

    /**
     * Checks whether the given {@link AndroidBrowserWindowCreateParams} can be used to create a
     * window on Android.
     *
     * <p>This method should be the only place to verify {@link AndroidBrowserWindowCreateParams}.
     * Other code can assume {@link AndroidBrowserWindowCreateParams} is valid.
     */
    private static boolean canCreateBrowserWindow(AndroidBrowserWindowCreateParams createParams) {
        @BrowserWindowType int browserWindowType = createParams.getWindowType();
        if (browserWindowType != BrowserWindowType.NORMAL
                && browserWindowType != BrowserWindowType.POPUP) {
            Log.e(TAG, String.format(Locale.US, "Unsupported window type: %d", browserWindowType));
            return false;
        }

        @WindowShowState.EnumType int initialShowState = createParams.getInitialShowState();
        if (initialShowState == WindowShowState.FULLSCREEN) {
            Log.e(TAG, String.format(Locale.US, "Unsupported show state: %d", initialShowState));
            return false;
        }

        return true;
    }
}
