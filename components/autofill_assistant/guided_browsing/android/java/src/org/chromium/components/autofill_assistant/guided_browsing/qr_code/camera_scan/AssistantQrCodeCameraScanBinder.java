// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import android.widget.TextView;

import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

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
        private final AssistantQrCodeCameraPreviewOverlay mCameraPreviewOverlay;

        public ViewHolder(AssistantQrCodeCameraScanView cameraScanView) {
            mCameraScanView = cameraScanView;
            mTitleView = cameraScanView.getRootView().findViewById(R.id.toolbar_title);
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
        } else if (propertyKey == AssistantQrCodeCameraScanModel.TOOLBAR_TITLE) {
            viewHolder.mTitleView.setText(model.get(AssistantQrCodeCameraScanModel.TOOLBAR_TITLE));
        } else if (propertyKey == AssistantQrCodeCameraScanModel.OVERLAY_INSTRUCTION_TEXT) {
            viewHolder.mCameraPreviewOverlay.setInstructionText(
                    model.get(AssistantQrCodeCameraScanModel.OVERLAY_INSTRUCTION_TEXT));
        } else if (propertyKey == AssistantQrCodeCameraScanModel.OVERLAY_SECURITY_TEXT) {
            viewHolder.mCameraPreviewOverlay.setSecurityText(
                    model.get(AssistantQrCodeCameraScanModel.OVERLAY_SECURITY_TEXT));
        } else {
            assert false : "Unhandled property detected in AssistantQrCodeCameraScanBinder!";
        }
    }
}