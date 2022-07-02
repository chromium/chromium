// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import android.view.View;
import android.widget.TextView;

import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.widget.ButtonCompat;

/**
 * This class is responsible for pushing updates to the Autofill Assistant UI for QR Code Camera
 * Scan. These updates are pulled from the {@link AssistantQrCodeCameraScanModel} when a
 * notification of an update is received.
 */
class AssistantQrCodeCameraScanBinder implements ViewBinder<AssistantQrCodeCameraScanModel,
        AssistantQrCodeCameraScanBinder.ViewHolder, PropertyKey> {
    /**
     * A wrapper class that holds the different views of the QR Code Camera Scan UI.
     */
    static class ViewHolder {
        private final AssistantQrCodeCameraScanView mCameraScanView;
        private final TextView mTitleView;
        private final View mCameraPermissionsView;
        private final View mOpenSettingsView;
        private final AssistantQrCodeCameraPreviewOverlay mCameraPreviewOverlay;

        public ViewHolder(AssistantQrCodeCameraScanView cameraScanView) {
            mCameraScanView = cameraScanView;
            mTitleView = cameraScanView.getRootView().findViewById(R.id.toolbar_title);
            mCameraPermissionsView = cameraScanView.getCameraPermissionView();
            mOpenSettingsView = cameraScanView.getOpenSettingsView();
            mCameraPreviewOverlay = cameraScanView.getCameraPreviewOverlay();
        }
    }

    @Override
    public void bind(
            AssistantQrCodeCameraScanModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (propertyKey == AssistantQrCodeCameraScanModel.DELEGATE) {
            // Do nothing. Subsequent notifications will be sent to the new delegate.
        } else if (propertyKey == AssistantQrCodeCameraScanModel.IS_ON_FOREGROUND) {
            viewHolder.mCameraScanView.onForegroundChanged(
                    model.get(AssistantQrCodeCameraScanModel.IS_ON_FOREGROUND));
        } else if (propertyKey == AssistantQrCodeCameraScanModel.HAS_CAMERA_PERMISSION) {
            viewHolder.mCameraScanView.cameraPermissionsChanged(
                    model.get(AssistantQrCodeCameraScanModel.HAS_CAMERA_PERMISSION));
        } else if (propertyKey == AssistantQrCodeCameraScanModel.CAN_PROMPT_FOR_CAMERA_PERMISSION) {
            viewHolder.mCameraScanView.canPromptForPermissionChanged(
                    model.get(AssistantQrCodeCameraScanModel.CAN_PROMPT_FOR_CAMERA_PERMISSION));
        } else if (propertyKey == AssistantQrCodeCameraScanModel.TOOLBAR_TITLE) {
            viewHolder.mTitleView.setText(model.get(AssistantQrCodeCameraScanModel.TOOLBAR_TITLE));
        } else if (propertyKey == AssistantQrCodeCameraScanModel.PERMISSION_TEXT) {
            TextView permissionsTextView =
                    viewHolder.mCameraPermissionsView.findViewById(R.id.permission_text);
            permissionsTextView.setText(model.get(AssistantQrCodeCameraScanModel.PERMISSION_TEXT));
        } else if (propertyKey == AssistantQrCodeCameraScanModel.PERMISSION_BUTTON_TEXT) {
            ButtonCompat permissionButton =
                    viewHolder.mCameraPermissionsView.findViewById(R.id.permission_button);
            permissionButton.setText(
                    model.get(AssistantQrCodeCameraScanModel.PERMISSION_BUTTON_TEXT));
        } else if (propertyKey == AssistantQrCodeCameraScanModel.OPEN_SETTINGS_TEXT) {
            TextView openSettingsTextView =
                    viewHolder.mOpenSettingsView.findViewById(R.id.permission_text);
            openSettingsTextView.setText(
                    model.get(AssistantQrCodeCameraScanModel.OPEN_SETTINGS_TEXT));
        } else if (propertyKey == AssistantQrCodeCameraScanModel.OPEN_SETTINGS_BUTTON_TEXT) {
            ButtonCompat openSettingsButton =
                    viewHolder.mOpenSettingsView.findViewById(R.id.permission_button);
            openSettingsButton.setText(
                    model.get(AssistantQrCodeCameraScanModel.OPEN_SETTINGS_BUTTON_TEXT));
        } else if (propertyKey == AssistantQrCodeCameraScanModel.OVERLAY_TITLE) {
            viewHolder.mCameraPreviewOverlay.setTextInstructions(
                    model.get(AssistantQrCodeCameraScanModel.OVERLAY_TITLE));
        } else {
            assert false : "Unhandled property detected in AssistantQrCodeCameraScanBinder!";
        }
    }
}