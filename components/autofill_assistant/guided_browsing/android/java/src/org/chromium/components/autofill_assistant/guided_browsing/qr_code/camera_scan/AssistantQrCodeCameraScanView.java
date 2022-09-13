// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import android.content.Context;
import android.hardware.Camera.ErrorCallback;
import android.hardware.Camera.PreviewCallback;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.components.autofill_assistant.guided_browsing.LayoutUtils;
import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * Manages the Android View representing the QR Code Camera scan panel.
 */
class AssistantQrCodeCameraScanView {
    /** Interface used to delegate various user interactions to the coordinator. */
    public interface Delegate {
        /** Called when user cancels the QR Code Scanning */
        void onScanCancelled();
    }

    private final Context mContext;
    private final PreviewCallback mCameraPreviewCallback;
    private final ErrorCallback mCameraErrorCallback;
    private final AssistantQrCodeCameraScanView.Delegate mViewDelegate;

    private final View mRootView;
    // Will be used to update the main body of the view based on state.
    private final FrameLayout mBodyView;
    private final View mCameraPermissionView;
    private final AssistantQrCodeCameraPreviewOverlay mCameraPreviewOverlay;

    private boolean mHasCameraPermission;
    private boolean mIsOnForeground;
    private AssistantQrCodeCameraPreview mCameraPreview;

    /**
     * The AssistantQrCodeCameraScanView constructor.
     */
    public AssistantQrCodeCameraScanView(Context context, View cameraPermissionView,
            PreviewCallback cameraPreviewCallback, ErrorCallback cameraErrorCallback,
            AssistantQrCodeCameraScanView.Delegate delegate) {
        mContext = context;
        mCameraPreviewCallback = cameraPreviewCallback;
        mCameraErrorCallback = cameraErrorCallback;
        mViewDelegate = delegate;

        mBodyView = new FrameLayout(context);
        mRootView = createRootView();
        mCameraPermissionView = cameraPermissionView;
        mCameraPreviewOverlay = new AssistantQrCodeCameraPreviewOverlay(context);
    }

    public View getRootView() {
        return mRootView;
    }

    public AssistantQrCodeCameraPreviewOverlay getCameraPreviewOverlay() {
        return mCameraPreviewOverlay;
    }

    /**
     * Creates the root view for the QR Code Camera Scan Dialog.
     */
    private View createRootView() {
        View dialogView = LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_qr_code_dialog, /* root= */ null);

        ChromeImageButton closeButton = dialogView.findViewById(R.id.close_button);
        closeButton.setOnClickListener(v -> { mViewDelegate.onScanCancelled(); });

        FrameLayout layout = dialogView.findViewById(R.id.qr_code_view);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
        layout.addView(mBodyView, params);

        return dialogView;
    }

    /**
     * Updates the state of the view based on the updated value of |isOnForeground|.
     *
     * @param isOnForeground Indicates whether this component UI is currently on foreground.
     */
    public void onForegroundChanged(Boolean isOnForeground) {
        mIsOnForeground = isOnForeground;
        if (!mIsOnForeground && mCameraPreview != null) {
            mCameraPreview.stopCamera();
        } else {
            displayUpdatedView();
        }
    }

    /**
     * Updates the state of the view based on the updated value of |hasCameraPermission|.
     *
     * @param hasCameraPermission Indicates whether this component UI has camera permission or not.
     */
    public void onPermissionsChanged(boolean hasCameraPermission) {
        mHasCameraPermission = hasCameraPermission;
        displayUpdatedView();
    }

    /**
     * Update the view based on various state parameters:
     * - app is in the foreground
     * - user has given camera permission
     * - user can be prompted for camera permission.
     */
    private void displayUpdatedView() {
        // The scan tab is not in the foreground so don't do any rendering.
        if (!mIsOnForeground) {
            return;
        }

        if (mHasCameraPermission && mCameraPreview != null) {
            mCameraPreview.startCamera();
        } else if (mHasCameraPermission && mCameraPreview == null) {
            displayCameraPreview();
        } else {
            displayCameraPermissionView();
        }
    }

    /**
     * Displays the camera preview with overlay. Caller should check that we already have the
     * camera permissions.
     */
    public void displayCameraPreview() {
        mBodyView.removeAllViews();
        stopCamera();

        mCameraPreview = new AssistantQrCodeCameraPreview(
                mContext, mCameraPreviewCallback, mCameraErrorCallback);
        mBodyView.addView(mCameraPreview);
        mBodyView.addView(mCameraPreviewOverlay);

        mCameraPreview.startCamera();
    }

    /**
     * Displays the permission dialog. Caller should check that the user can be prompted and hasn't
     * permanently denied permission.
     */
    private void displayCameraPermissionView() {
        mBodyView.removeAllViews();
        mBodyView.addView(mCameraPermissionView);
    }

    /**
     * Stop the camera.
     */
    public void stopCamera() {
        if (mCameraPreview != null) {
            mCameraPreview.stopCamera();
            mCameraPreview = null;
        }
    }
}
