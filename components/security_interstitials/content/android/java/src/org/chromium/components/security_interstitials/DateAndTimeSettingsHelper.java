// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.security_interstitials;

import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Helper for opening date and time settings.
 */
@JNINamespace("security_interstitials")
public abstract class DateAndTimeSettingsHelper {
    private DateAndTimeSettingsHelper() {}

    /**
     * Opens date and time in Android settings.
     *
     */
    @CalledByNative
    static void openDateAndTimeSettings() {
        Intent intent = new Intent(android.provider.Settings.ACTION_DATE_SETTINGS);

        try {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ContextUtils.getApplicationContext().startActivity(intent);
        } catch (android.content.ActivityNotFoundException ex) {
            // If it doesn't work, avoid crashing.
        }
    }
}
