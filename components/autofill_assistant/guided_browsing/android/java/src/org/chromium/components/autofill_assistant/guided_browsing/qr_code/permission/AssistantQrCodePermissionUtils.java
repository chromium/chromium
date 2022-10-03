// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

import android.content.pm.PackageManager;

import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;

/**
 * AssistantQrCodePermissionUtils provides various utility fucnctions around permissions needed
 * for Qr Code scanning.
 */
public class AssistantQrCodePermissionUtils {
    /** Returns whether the user has granted permissions. */
    public static boolean hasPermission(
            WindowAndroid windowAndroid, AssistantQrCodePermissionType requiredPermission) {
        return windowAndroid.hasPermission(requiredPermission.getAndroidPermission());
    }

    /** Returns whether the user can be prompted for permissions. */
    public static boolean canPromptForPermission(
            WindowAndroid windowAndroid, AssistantQrCodePermissionType requiredPermission) {
        return windowAndroid.canRequestPermission(requiredPermission.getAndroidPermission());
    }

    /**
     * Prompts the user for the permission. Processes the results and updates {@permissionModel}
     * values accordingly.
     */
    public static void promptForPermission(WindowAndroid windowAndroid,
            AssistantQrCodePermissionType requiredPermission,
            AssistantQrCodePermissionModel permissionModel) {
        final PermissionCallback callback = new PermissionCallback() {
            // Handle the results from prompting the user for permission.
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                // No results were produced (Does this ever happen?)
                if (grantResults.length == 0) {
                    return;
                }
                if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    permissionModel.setHasPermission(true);
                } else {
                    // The order in which these fields are important because it causes updates to
                    // the view. CanPromptForPermission must be updated first so that it doesn't
                    // cause the view to be updated twice creating a flicker effect.
                    if (!windowAndroid.canRequestPermission(
                                requiredPermission.getAndroidPermission())) {
                        permissionModel.setCanPromptForPermission(false);
                    }
                    permissionModel.setHasPermission(false);
                }
            }
        };

        windowAndroid.requestPermissions(
                new String[] {requiredPermission.getAndroidPermission()}, callback);
    }
}