// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.security_interstitials;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.provider.Settings;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;

/** Helper for opening date and time settings. */
@JNINamespace("security_interstitials")
@NullMarked
public abstract class DateAndTimeSettingsHelper {
    private DateAndTimeSettingsHelper() {}

    /** Opens date and time in Android settings. */
    @CalledByNative
    static void openDateAndTimeSettings() {
        Intent intent = new Intent(Settings.ACTION_DATE_SETTINGS);

        try {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ContextUtils.getApplicationContext().startActivity(intent);
        } catch (ActivityNotFoundException ex) {
            // If it doesn't work, avoid crashing.
        }
    }
}
