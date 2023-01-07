// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.qr_code;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan.AssistantQrCodeCameraScanModel;

/**
 * Wrapper around the |AssistantQrCodeCameraScanModel| to manage the state for the QR Code Camera
 * Scan UI from the native code.
 */
@JNINamespace("autofill_assistant")
public class AssistantQrCodeCameraScanModelWrapper {
    private final AssistantQrCodeCameraScanModel mCameraScanModel;

    /**
     * The AssistantQrCodeCameraScanWrapperModel constructor.
     */
    @CalledByNative
    public AssistantQrCodeCameraScanModelWrapper() {
        mCameraScanModel = new AssistantQrCodeCameraScanModel();
    }

    /**
     * Returns the underlying AssistantQrCodeCameraScanModel object.
     */
    AssistantQrCodeCameraScanModel getCameraScanModel() {
        return mCameraScanModel;
    }

    @CalledByNative
    private void setDelegate(AssistantQrCodeNativeDelegate delegate) {
        mCameraScanModel.setDelegate(delegate);
    }

    @CalledByNative
    private void setToolbarTitle(String text) {
        mCameraScanModel.setToolbarTitle(text);
    }

    @CalledByNative
    private void setPermissionText(String text) {
        mCameraScanModel.setPermissionText(text);
    }

    @CalledByNative
    private void setPermissionButtonText(String text) {
        mCameraScanModel.setPermissionButtonText(text);
    }

    @CalledByNative
    private void setOpenSettingsText(String text) {
        mCameraScanModel.setOpenSettingsText(text);
    }

    @CalledByNative
    private void setOpenSettingsButtonText(String text) {
        mCameraScanModel.setOpenSettingsButtonText(text);
    }

    @CalledByNative
    private void setOverlayInstructionText(String text) {
        mCameraScanModel.setOverlayInstructionText(text);
    }

    @CalledByNative
    private void setOverlaySecurityText(String text) {
        mCameraScanModel.setOverlaySecurityText(text);
    }
}
