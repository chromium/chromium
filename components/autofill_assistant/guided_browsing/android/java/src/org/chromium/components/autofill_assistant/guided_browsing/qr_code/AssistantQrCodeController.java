// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code;

import android.app.Activity;

import org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan.AssistantQrCodeCameraScanDialog;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan.AssistantQrCodeCameraScanModel;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker.AssistantQrCodeImagePickerDialog;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker.AssistantQrCodeImagePickerModel;
import org.chromium.ui.base.WindowAndroid;

/** Controller to expose QR Code Scan functionality. */
public class AssistantQrCodeController {
    /** Prompts the user for QR Code Scanning via Camera Preview. */
    public static void promptQrCodeCameraScan(
            Activity activity, WindowAndroid windowAndroid, AssistantQrCodeCameraScanModel model) {
        AssistantQrCodeCameraScanDialog.newInstance(activity, windowAndroid, model)
                .show(activity.getFragmentManager(), /* tag= */ null);
    }

    /** Prompts the user for QR Code Scanning via Image Picker. */
    public static void promptQrCodeImagePicker(
            Activity activity, WindowAndroid windowAndroid, AssistantQrCodeImagePickerModel model) {
        AssistantQrCodeImagePickerDialog.newInstance(activity, windowAndroid, model)
                .show(activity.getFragmentManager(), /* tag= */ null);
    }
}