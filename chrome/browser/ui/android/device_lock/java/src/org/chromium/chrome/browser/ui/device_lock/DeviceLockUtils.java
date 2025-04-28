// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.device_lock;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.build.annotations.NullMarked;

@NullMarked
public class DeviceLockUtils {
    static boolean isDeviceLockCreationIntentSupported(Context context) {
        return new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD)
                        .resolveActivity(context.getPackageManager())
                != null;
    }

    static Intent createDeviceLockDirectlyIntent() {
        return new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD);
    }

    static Intent createDeviceLockThroughOSSettingsIntent() {
        return new Intent(Settings.ACTION_SECURITY_SETTINGS);
    }

    // TODO(crbug.com/352735671): Remove this once UNO_FOR_AUTO is launched, amd move the layout
    // changes to the corresponding XMLs.
    static void updateDialogSubviewMargins(View view) {
        int marginPx =
                view.getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.device_lock_dialog_horizontal_margin);
        MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
        layoutParams.setMargins(
                layoutParams.leftMargin + marginPx,
                layoutParams.topMargin,
                layoutParams.rightMargin + marginPx,
                layoutParams.bottomMargin);
        view.setLayoutParams(layoutParams);
    }
}
