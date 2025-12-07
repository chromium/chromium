// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions.nfc;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.nfc.NfcAdapter;
import android.os.Process;
import android.provider.Settings;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceRunnable;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Provides methods for querying NFC sytem-level setting on Android.
 *
 * This class should be used only on the UI thread.
 */
@NullMarked
public class NfcSystemLevelSetting {
    private static @Nullable Boolean sNfcSupportForTesting;
    private static @Nullable Boolean sSystemNfcSettingForTesting;

    @CalledByNative
    public static boolean isNfcAccessPossible() {
        if (sNfcSupportForTesting != null) {
            return sNfcSupportForTesting;
        }

        Context context = ContextUtils.getApplicationContext();
        int permission =
                context.checkPermission(Manifest.permission.NFC, Process.myPid(), Process.myUid());
        if (permission != PackageManager.PERMISSION_GRANTED) {
            return false;
        }

        NfcAdapter nfcAdapter = NfcAdapter.getDefaultAdapter(context);
        return nfcAdapter != null;
    }

    @CalledByNative
    public static boolean isNfcSystemLevelSettingEnabled() {
        if (sSystemNfcSettingForTesting != null) {
            return sSystemNfcSettingForTesting;
        }

        if (!isNfcAccessPossible()) return false;

        NfcAdapter nfcAdapter = NfcAdapter.getDefaultAdapter(ContextUtils.getApplicationContext());
        return nfcAdapter.isEnabled();
    }

    @CalledByNative
    private static void promptToEnableNfcSystemLevelSetting(
            WebContents webContents, JniOnceRunnable callback) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) {
            // Consuming code may not expect a sync callback to happen.
            PostTask.postTask(TaskTraits.UI_DEFAULT, callback);
            return;
        }

        NfcSystemLevelPrompt prompt = new NfcSystemLevelPrompt();
        prompt.show(window, callback);
    }

    public static @Nullable Intent getNfcSystemLevelSettingIntent() {
        Intent intent = new Intent(Settings.ACTION_NFC_SETTINGS);
        Context context = ContextUtils.getApplicationContext();
        if (intent.resolveActivity(context.getPackageManager()) == null) {
            return null;
        }
        return intent;
    }

    /** Disable/enable Android NFC setting for testing use only. */
    public static void setNfcSettingForTesting(Boolean enabled) {
        sSystemNfcSettingForTesting = enabled;
        ResettersForTesting.register(() -> sSystemNfcSettingForTesting = null);
    }

    /** Disable/enable Android NFC support for testing use only. */
    public static void setNfcSupportForTesting(Boolean enabled) {
        sNfcSupportForTesting = enabled;
        ResettersForTesting.register(() -> sNfcSupportForTesting = null);
    }
}
