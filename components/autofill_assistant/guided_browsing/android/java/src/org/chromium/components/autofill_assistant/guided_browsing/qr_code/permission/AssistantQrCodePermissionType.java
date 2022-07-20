// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

import android.Manifest.permission;

import org.chromium.ui.permissions.PermissionConstants;

public enum AssistantQrCodePermissionType {
    // List of permissions should be mentioned here
    CAMERA(permission.CAMERA, "camera_img"),
    READ_MEDIA_IMAGES(PermissionConstants.READ_MEDIA_IMAGES, "folder_img"),
    READ_EXTERNAL_STORAGE(permission.READ_EXTERNAL_STORAGE, "folder_img");

    private String mAndroidPermission;
    private String mAndroidPermissionImage;

    public String getAndroidPermission() {
        return this.mAndroidPermission;
    }

    public String getAndroidPermissionImage() {
        return this.mAndroidPermissionImage;
    }

    private AssistantQrCodePermissionType(String permission, String permissionImage) {
        this.mAndroidPermission = permission;
        this.mAndroidPermissionImage = permissionImage;
    }
}
