// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.device_lock;

import android.content.Context;

import androidx.annotation.StringDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Allows for launching {@link DeviceLockActivity} in modularized code. */
@NullMarked
public interface DeviceLockActivityLauncher {

    /**
     * Enum representing the flow user took to arrive at the device lock UI that corresponds with
     * the right histogram name.
     */
    // LINT.IfChange(DeviceLockSource)
    @StringDef({Source.FIRST_RUN, Source.ACCOUNT_PICKER, Source.AUTOFILL, Source.FULLSCREEN_SIGNIN})
    @Retention(RetentionPolicy.SOURCE)
    @interface Source {
        String FIRST_RUN = "FirstRun";
        String ACCOUNT_PICKER = "AccountPicker";
        String AUTOFILL = "Autofill";
        String FULLSCREEN_SIGNIN = "FullscreenSignin";
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/android/histograms.xml:DeviceLockSource)

    static boolean isSignInFlow(@Source String source) {
        return source.equals(Source.FIRST_RUN)
                || source.equals(Source.ACCOUNT_PICKER)
                || source.equals(Source.FULLSCREEN_SIGNIN);
    }

    /**
     * Launches the {@link DeviceLockActivity} to set a device lock for data privacy.
     *
     * @param context The context to launch the {@link DeviceLockActivity} with.
     * @param selectedAccount The account that will be used for the reauthentication challenge, or
     *     null if reauthentication is not needed.
     * @param requireDeviceLockReauthentication Whether or not the reauthentication of the device
     *     lock credentials should be required (if a device lock is already present).
     * @param windowAndroid The host activity's {@link WindowAndroid}.
     * @param callback A callback to run after the {@link DeviceLockActivity} finishes.
     * @param flow Which flow the user took to arrive at the device lock UI.
     */
    void launchDeviceLockActivity(
            Context context,
            @Nullable String selectedAccount,
            boolean requireDeviceLockReauthentication,
            WindowAndroid windowAndroid,
            WindowAndroid.IntentCallback callback,
            @Source String source);
}
