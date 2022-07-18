// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

import android.Manifest.permission;

public enum AssistantQrCodePermissionType {
    // List of permissions should be mentioned here
    CAMERA(permission.CAMERA);

    private String mAndroidPermission;

    public String getAndroidPermission() {
        return this.mAndroidPermission;
    }

    private AssistantQrCodePermissionType(String permission) {
        this.mAndroidPermission = permission;
    }
}