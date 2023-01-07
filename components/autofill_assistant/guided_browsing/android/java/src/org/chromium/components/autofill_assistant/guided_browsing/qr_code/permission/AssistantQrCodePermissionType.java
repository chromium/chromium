// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

import android.Manifest.permission;

import androidx.annotation.DrawableRes;

import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.ui.permissions.PermissionConstants;

public enum AssistantQrCodePermissionType {
    // List of permissions should be mentioned here
    CAMERA(permission.CAMERA, R.drawable.videocam_img, new AssistantQrCodeCameraPermissionMetric()),
    READ_MEDIA_IMAGES(PermissionConstants.READ_MEDIA_IMAGES, R.drawable.folder_img,
            new AssistantQrCodeReadImagesPermissionMetric()),
    READ_EXTERNAL_STORAGE(permission.READ_EXTERNAL_STORAGE, R.drawable.folder_img,
            new AssistantQrCodeReadImagesPermissionMetric());

    private String mAndroidPermission;
    private @DrawableRes int mAndroidPermissionImage;
    private AssistantQrCodePermissionMetric mAndroidPermissionMetric;

    public String getAndroidPermission() {
        return this.mAndroidPermission;
    }

    public @DrawableRes int getAndroidPermissionImage() {
        return this.mAndroidPermissionImage;
    }

    public AssistantQrCodePermissionMetric getAndroidPermissionMetric() {
        return this.mAndroidPermissionMetric;
    }

    private AssistantQrCodePermissionType(String permission, @DrawableRes int permissionImage,
            AssistantQrCodePermissionMetric permissionMetric) {
        this.mAndroidPermission = permission;
        this.mAndroidPermissionImage = permissionImage;
        this.mAndroidPermissionMetric = permissionMetric;
    }
}
