// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.lens;

import android.graphics.Bitmap;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UserData;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.UUID;

/**
 * Coordinator for the Lens Overlay feature. Stays alive for the lifetime of the {@link Tab} to
 * manage the JNI bridge and UI state.
 */
@JNINamespace("lens")
@NullMarked
public class LensOverlayCoordinator implements UserData {
    private static final String TAG = "LensOverlay";

    private final Tab mTab;
    private long mNativeLensOverlayControllerAndroid;

    /**
     * Returns the {@link LensOverlayCoordinator} for the {@link Tab}, creating it if needed.
     *
     * @param tab The tab to get the coordinator for.
     * @return The {@link LensOverlayCoordinator} for the tab.
     */
    public static LensOverlayCoordinator getOrCreateForTab(Tab tab) {
        LensOverlayCoordinator coordinator =
                tab.getUserDataHost().getUserData(LensOverlayCoordinator.class);
        if (coordinator == null) {
            coordinator = new LensOverlayCoordinator(tab);
            tab.getUserDataHost().setUserData(LensOverlayCoordinator.class, coordinator);
        }
        return coordinator;
    }

    private LensOverlayCoordinator(Tab tab) {
        mTab = tab;
        WebContents webContents = tab.getWebContents();
        assert webContents != null;
        mNativeLensOverlayControllerAndroid =
                LensOverlayCoordinatorJni.get().init(this, webContents);
    }

    /**
     * Starts the Lens Overlay.
     *
     * @param invocationSource The source of the invocation.
     * @return Whether the overlay was successfully started.
     */
    public boolean start(@LensOverlayInvocationSource int invocationSource) {
        if (LensOverlayTabHelper.isOverlayShowing(mTab)) {
            return false;
        }

        setShowing(true);

        // TODO(b/510446979): Implement TabObserver to call setShowing(false) and teardown
        // the UI if the user navigates to a new page or back/forward while the overlay
        // is active or starting up.

        Log.i(TAG, "Starting Lens Overlay");

        // The flow:
        // 1. Java calls C++ showUI().
        // 2. C++ triggers asynchronous screenshot capture.
        // 3. Upon completion, C++ invokes Java's onScreenshotCaptured().
        // Failure cases:
        // - If capture fails to start, start() returns failure immediately.
        // - If capture fails asynchronously (e.g., GPU crash), onCaptureError() is called.
        if (!LensOverlayCoordinatorJni.get()
                .showUI(mNativeLensOverlayControllerAndroid, invocationSource)) {
            Log.e(TAG, "Failed to show Lens Overlay UI");
            setShowing(false);
            return false;
        }
        return true;
    }

    /**
     * Called by the C++ controller when the window screenshot is ready. If the C++ capture fails,
     * this method is never called and the feature gracefully aborts.
     *
     * @param bitmap The captured screenshot of the Chrome window.
     */
    @CalledByNative
    void onScreenshotCaptured(@JniType("SkBitmap") Bitmap bitmap) {
        Log.i(TAG, "onScreenshotCaptured called in Java");

        WebContents webContents = mTab.getWebContents();
        if (webContents == null) {
            setShowing(false);
            return;
        }
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) {
            setShowing(false);
            return;
        }

        // Collect physical screen metrics on the UI thread before hopping to background.
        LensOverlayImageHelper.LensOverlayScreenMetrics metrics =
                LensOverlayImageHelper.getScreenMetrics(window);
        if (metrics == null) {
            // Fallback: If metrics can't be determined, use the windowshot as-is.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT, () -> saveCompositedImageAndLaunch(window, bitmap));
            return;
        }

        // Hop to background thread for heavy image processing.
        PostTask.postTask(
                TaskTraits.USER_VISIBLE, () -> compositeImageInBackground(window, bitmap, metrics));
    }

    @VisibleForTesting
    void compositeImageInBackground(
            WindowAndroid window,
            Bitmap windowshot,
            LensOverlayImageHelper.LensOverlayScreenMetrics metrics) {
        ThreadUtils.assertOnBackgroundThread();
        Bitmap composited = LensOverlayImageHelper.compositeBitmap(metrics, windowshot);

        // Hop back to UI thread to interact with ShareImageFileUtils.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT, () -> saveCompositedImageAndLaunch(window, composited));
    }

    @VisibleForTesting
    void saveCompositedImageAndLaunch(WindowAndroid window, Bitmap composited) {
        ThreadUtils.assertOnUiThread();
        ShareImageFileUtils.generateTemporaryUriFromBitmap(
                UUID.randomUUID().toString(),
                composited,
                (Uri imageUri) -> {
                    composited.recycle();
                    if (imageUri != null) {
                        launchLensIntent(window, imageUri);
                    } else {
                        Log.e(TAG, "Failed to process image for Lens Overlay");
                        setShowing(false);
                    }
                });
    }

    private void launchLensIntent(WindowAndroid window, Uri imageUri) {
        String pageUrl = mTab.getUrl() != null ? mTab.getUrl().getSpec() : "";
        LensIntentParams params =
                new LensIntentParams.Builder(LensEntryPoint.CHROME_LENS_OVERLAY, mTab.isIncognito())
                        .withImageUri(imageUri)
                        .withPageUrl(pageUrl)
                        .withSrcUrl("")
                        .withImageTitleOrAltText("")
                        .build();
        LensController.getInstance().startLens(window, params);

        // For the AGSA intent-based flow, we dismiss the overlay state immediately after
        // handing off to the external app.
        setShowing(false);
    }

    /** Called by the C++ controller when an asynchronous error occurs during capture. */
    @CalledByNative
    void onCaptureError() {
        Log.e(TAG, "onCaptureError called in Java");
        setShowing(false);
    }

    private void setShowing(boolean showing) {
        LensOverlayTabHelper.setOverlayShowing(mTab, showing);
    }

    @Override
    public void destroy() {
        if (mNativeLensOverlayControllerAndroid != 0) {
            LensOverlayCoordinatorJni.get().destroy(mNativeLensOverlayControllerAndroid);
            mNativeLensOverlayControllerAndroid = 0;
        }
        setShowing(false);
    }

    @NativeMethods
    interface Natives {
        long init(
                LensOverlayCoordinator caller,
                @JniType("content::WebContents*") WebContents webContents);

        boolean showUI(
                long nativeLensOverlayControllerAndroid,
                @LensOverlayInvocationSource int invocationSource);

        void destroy(long nativeLensOverlayControllerAndroid);
    }
}
