// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.qr_code;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker.AssistantQrCodeImagePickerModel;

/**
 * Wrapper around the |AssistantQrCodeImagePickerModel| to manage the state for the QR Code Image
 * Picker UI from the native code.
 */
@JNINamespace("autofill_assistant")
public class AssistantQrCodeImagePickerModelWrapper {
    private final AssistantQrCodeImagePickerModel mImagePickerModel;

    /**
     * The AssistantQrCodeImagePickerWrapperModel constructor.
     */
    @CalledByNative
    public AssistantQrCodeImagePickerModelWrapper() {
        mImagePickerModel = new AssistantQrCodeImagePickerModel();
    }

    /**
     * Returns the underlying AssistantQrCodeImagePickerModel object.
     */
    AssistantQrCodeImagePickerModel getImagePickerModel() {
        return mImagePickerModel;
    }

    @CalledByNative
    private void setDelegate(AssistantQrCodeNativeDelegate delegate) {
        mImagePickerModel.setDelegate(delegate);
    }

    @CalledByNative
    private void setToolbarTitle(String text) {
        mImagePickerModel.setToolbarTitle(text);
    }

    @CalledByNative
    private void setPermissionText(String text) {
        mImagePickerModel.setPermissionText(text);
    }

    @CalledByNative
    private void setPermissionButtonText(String text) {
        mImagePickerModel.setPermissionButtonText(text);
    }

    @CalledByNative
    private void setOpenSettingsText(String text) {
        mImagePickerModel.setOpenSettingsText(text);
    }

    @CalledByNative
    private void setOpenSettingsButtonText(String text) {
        mImagePickerModel.setOpenSettingsButtonText(text);
    }
}
