// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.hardware.Camera.ErrorCallback;
import android.hardware.Camera.PreviewCallback;
import android.net.Uri;
import android.provider.Settings;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.FrameLayout;

import org.chromium.components.autofill_assistant.guided_browsing.LayoutUtils;
import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * Manages the Android View representing the QR Code Camera scan panel.
 */
class AssistantQrCodeCameraScanView {
    /** Interface used to delegate various user interactions to the coordinator. */
    public interface Delegate {
        /** Called when user cancels the QR Code Scanning */
        void onScanCancelled();

        /** Prompts the user for camera permissions */
        void promptForCameraPermission();
    }

    private final Context mContext;
    private final PreviewCallback mCameraPreviewCallback;
    private final ErrorCallback mCameraErrorCallback;
    private final AssistantQrCodeCameraScanView.Delegate mViewDelegate;

    private final View mRootView;
    // Will be used to update the main body of the view based on state.
    private final FrameLayout mBodyView;
    private final View mCameraPermissionsView;
    private final View mOpenSettingsView;
    private final AssistantQrCodeCameraPreviewOverlay mCameraPreviewOverlay;

    private boolean mHasCameraPermission;
    private boolean mCanPromptForPermission;
    private boolean mIsOnForeground;
    private AssistantQrCodeCameraPreview mCameraPreview;

    /**
     * The AssistantQrCodeCameraScanView constructor.
     */
    public AssistantQrCodeCameraScanView(Context context, PreviewCallback cameraPreviewCallback,
            ErrorCallback cameraErrorCallback, AssistantQrCodeCameraScanView.Delegate delegate) {
        mContext = context;
        mCameraPreviewCallback = cameraPreviewCallback;
        mCameraErrorCallback = cameraErrorCallback;
        mViewDelegate = delegate;

        mBodyView = new FrameLayout(context);
        mRootView = createRootView();
        mCameraPermissionsView = createCameraPermissionView();
        mOpenSettingsView = createOpenSettingsView();
        mCameraPreviewOverlay = new AssistantQrCodeCameraPreviewOverlay(context);
    }

    public View getRootView() {
        return mRootView;
    }

    public View getCameraPermissionView() {
        return mCameraPermissionsView;
    }

    public View getOpenSettingsView() {
        return mOpenSettingsView;
    }

    public AssistantQrCodeCameraPreviewOverlay getCameraPreviewOverlay() {
        return mCameraPreviewOverlay;
    }

    /**
     * Creates the root view for the QR Code Camera Scan Dialog.
     */
    private View createRootView() {
        View dialogView = LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_qr_code_camera_scan_dialog, /* root= */ null);

        ChromeImageButton closeButton = dialogView.findViewById(R.id.close_button);
        closeButton.setOnClickListener(v -> { mViewDelegate.onScanCancelled(); });

        FrameLayout layout = dialogView.findViewById(R.id.qr_code_view);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
        layout.addView(mBodyView, params);

        return dialogView;
    }

    /**
     * Creates a view that prompts the user ot provide camera permissions.
     */
    private View createCameraPermissionView() {
        View cameraPermissionsView = LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_qr_code_camera_scan_permission_layout,
                /* root= */ null);

        ButtonCompat permissionButton = cameraPermissionsView.findViewById(R.id.permission_button);
        permissionButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mViewDelegate.promptForCameraPermission();
            }
        });

        return cameraPermissionsView;
    }

    /**
     * Creates a view that opens the settings page for the app and allows the user to to update
     * permissions including give the app camera permission.
     */
    private View createOpenSettingsView() {
        View openSettingsView = LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_qr_code_camera_scan_permission_layout,
                /* root= */ null);

        ButtonCompat openSettingsButton = openSettingsView.findViewById(R.id.permission_button);
        openSettingsButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent openSettingsIntent = getAppInfoIntent(mContext.getPackageName());
                ((Activity) mContext).startActivity(openSettingsIntent);
            }
        });

        return openSettingsView;
    }

    /**
     * Updates on the state of the view based on the updated value of |hasCameraPermission|.
     *
     * @param hasCameraPermission Indicates whether camera permissions were granted.
     */
    public void cameraPermissionsChanged(Boolean hasCameraPermission) {
        // No change, nothing to do here
        if (mHasCameraPermission && hasCameraPermission) {
            return;
        }
        mHasCameraPermission = hasCameraPermission;
        updateView();
    }

    /**
     * Updates on the state of the view based on the updated value of |canPromptForPermission|.
     *
     * @param canPromptForPermission Indicates whether the user can be prompted for camera
     *            permission
     */
    public void canPromptForPermissionChanged(Boolean canPromptForPermission) {
        mCanPromptForPermission = canPromptForPermission;
        updateView();
    }

    /**
     * Updates on the state of the view based on the updated value of |isOnForeground|.
     *
     * @param isOnForeground Indicates whether this component UI is currently on foreground.
     */
    public void onForegroundChanged(Boolean isOnForeground) {
        mIsOnForeground = isOnForeground;
        if (!mIsOnForeground && mCameraPreview != null) {
            mCameraPreview.stopCamera();
        } else {
            updateView();
        }
    }

    /**
     * Update the view based on various state parameters:
     * - app is in the foreground
     * - user has given camera permission
     * - user can be prompted for camera permission.
     */
    private void updateView() {
        // The scan tab is not in the foreground so don't do any rendering.
        if (!mIsOnForeground) {
            return;
        }

        if (mHasCameraPermission && mCameraPreview != null) {
            mCameraPreview.startCamera();
        } else if (mHasCameraPermission && mCameraPreview == null) {
            displayCameraPreview();
        } else if (mCanPromptForPermission) {
            displayCameraPermissionsView();
        } else {
            displayOpenSettingsView();
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
    private void displayCameraPermissionsView() {
        mBodyView.removeAllViews();
        mBodyView.addView(mCameraPermissionsView);
    }

    /**
     * Displays the open settings dialog.
     */
    private void displayOpenSettingsView() {
        mBodyView.removeAllViews();
        mBodyView.addView(mOpenSettingsView);
    }

    /**
     * Returns an Intent to show the App Info page for the current app.
     */
    private Intent getAppInfoIntent(String packageName) {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.setData(new Uri.Builder().scheme("package").opaquePart(packageName).build());
        return intent;
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