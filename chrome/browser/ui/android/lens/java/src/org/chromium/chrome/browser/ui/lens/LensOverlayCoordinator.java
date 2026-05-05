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
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Coordinator for the Lens Overlay feature. */
@JNINamespace("lens")
@NullMarked
public class LensOverlayCoordinator {
    private static final String TAG = "LensOverlay";

    private final WebContents mOriginalWebContents;
    private long mNativeLensOverlayControllerAndroid;

    public LensOverlayCoordinator(WebContents webContents) {
        mOriginalWebContents = webContents;
        mNativeLensOverlayControllerAndroid =
                LensOverlayCoordinatorJni.get().init(this, webContents);
    }

    /** Starts the Lens Overlay. */
    public void start(@LensOverlayInvocationSource int invocationSource) {
        Log.i(TAG, "Starting Lens Overlay");

        // The flow:
        // 1. Java calls C++ showUI().
        // 2. C++ triggers asynchronous screenshot capture.
        // 3. When completion, C++ invokes Java's onScreenshotCaptured(), and flow continues.
        // Failure cases:
        // * If capture fails to start, call destroy() here and fail start() fails.
        // * If capture fails asynchronously (e.g., GPU crash), onScreenshotCaptured() is
        //   never called, and the flow gracefully/silently aborts.
        if (!LensOverlayCoordinatorJni.get()
                .showUI(mNativeLensOverlayControllerAndroid, invocationSource)) {
            Log.e(TAG, "Failed to show Lens Overlay UI");
            destroy();
        }
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

    /**
     * Displays the captured screenshot in a full-screen ImageView for debugging. TODO(b/493627069):
     * Remove this temporary debug UI and replace it with the actual intent handoff or the native
     * overlay UI.
     */
    private void showDebugImageView(Bitmap bitmap) {
        WindowAndroid window = mOriginalWebContents.getTopLevelNativeWindow();
        if (window == null) {
            return;
        }

        Activity activity = window.getActivity().get();
        if (activity == null) {
            return;
        }

        ImageView imageView = new ImageView(activity);
        imageView.setImageBitmap(bitmap);

        // Note: FIT_XY is used here for rapid prototyping, which might result in slight stretching
        // depending on the window-to-screen aspect ratio. This will be resolved when we implement
        // proper screenshot compositing.
        imageView.setScaleType(ImageView.ScaleType.FIT_XY);

        ViewGroup contentView = activity.findViewById(android.R.id.content);
        if (contentView != null) {
            contentView.addView(
                    imageView,
                    new ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT));

            // Temporary debug UI: Clicking anywhere on the screenshot will dismiss this view and
            // clean up the coordinator.
            imageView.setOnClickListener(
                    v -> {
                        contentView.removeView(imageView);
                        destroy();
                    });
        }
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

        boolean showUI(
                long nativeLensOverlayControllerAndroid,
                @LensOverlayInvocationSource int invocationSource);

        void destroy(long nativeLensOverlayControllerAndroid);
    }
}
