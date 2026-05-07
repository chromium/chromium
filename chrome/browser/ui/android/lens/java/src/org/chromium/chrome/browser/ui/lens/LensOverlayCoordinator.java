// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.lens;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

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
        showDebugImageView(bitmap);
    }

    /** Called by the C++ controller when an asynchronous error occurs during capture. */
    @CalledByNative
    void onCaptureError() {
        Log.e(TAG, "onCaptureError called in Java");
        setShowing(false);
    }

    /**
     * Displays the captured screenshot in a full-screen ImageView for debugging. TODO(b/493627069):
     * Remove this temporary debug UI and replace it with the actual intent handoff or the native
     * overlay UI.
     */
    private void showDebugImageView(Bitmap bitmap) {
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

        Activity activity = window.getActivity().get();
        if (activity == null) {
            setShowing(false);
            return;
        }

        ViewGroup contentView = activity.findViewById(android.R.id.content);
        if (contentView == null) {
            setShowing(false);
            return;
        }

        ImageView imageView = new ImageView(activity);
        imageView.setImageBitmap(bitmap);

        // Note: FIT_XY is used here for rapid prototyping, which might result in slight stretching
        // depending on the window-to-screen aspect ratio. This will be resolved when we implement
        // proper screenshot compositing.
        imageView.setScaleType(ImageView.ScaleType.FIT_XY);

        contentView.addView(
                imageView,
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        // Temporary debug UI: Clicking anywhere on the screenshot will dismiss this view and
        // clear the showing state.
        imageView.setOnClickListener(
                v -> {
                    contentView.removeView(imageView);
                    setShowing(false);
                });
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
