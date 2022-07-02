// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.utils;

import android.Manifest.permission;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;

import org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan.AssistantQrCodeCameraScanModel;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;

/**
 * AssistantQrCodePermissionUtils provides various utility fucnctions around permissions needed
 * for Qr Code scanning.
 */
public class AssistantQrCodePermissionUtils {
    /** Returns whether the user has granted camera permissions. */
    public static boolean hasCameraPermission(Context context) {
        return context.checkPermission(permission.CAMERA, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /** Returns whether the user can be prompted for camera permissions. */
    public static boolean canPromptForCameraPermission(WindowAndroid windowAndroid) {
        return windowAndroid.canRequestPermission(permission.CAMERA);
    }

    /**
     * Prompts the user for camera permission. Processes the results and updates {@cameraScanModel}
     * values accordingly.
     */
    public static void promptForCameraPermission(
            WindowAndroid windowAndroid, AssistantQrCodeCameraScanModel cameraScanModel) {
        final PermissionCallback callback = new PermissionCallback() {
            // Handle the results from prompting the user for camera permission.
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                // No results were produced (Does this ever happen?)
                if (grantResults.length == 0) {
                    return;
                }
                if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    cameraScanModel.setHasCameraPermission(true);
                } else {
                    // The order in which these fields are important because it causes updates to
                    // the view. CanPromptForPermission must be updated first so that it doesn't
                    // cause the view to be updated twice creating a flicker effect.
                    if (!windowAndroid.canRequestPermission(permission.CAMERA)) {
                        cameraScanModel.setCanPromptForCameraPermission(false);
                    }
                    cameraScanModel.setHasCameraPermission(false);
                }
            }
        };

        windowAndroid.requestPermissions(new String[] {permission.CAMERA}, callback);
    }
}
