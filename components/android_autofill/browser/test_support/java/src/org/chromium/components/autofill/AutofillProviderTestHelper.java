// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * The help class for Autofill Provider test to access the native code.
 */
@RequiresApi(Build.VERSION_CODES.O)
@JNINamespace("autofill")
public class AutofillProviderTestHelper {
    /**
     * Disable the download server for testing to avoid the server response affect the integration
     * tests. Must be called before WebContents is created.
     */
    public static void disableDownloadServerForTesting() {
        AutofillProviderTestHelperJni.get().disableDownloadServerForTesting();
    }

    /**
     * Simulate the primary server type only.
     */
    public static boolean simulateMainFrameAutofillServerResponseForTesting(
            WebContents webContents, String[] fieldIds, int[] fieldTypes) {
        return AutofillProviderTestHelperJni.get()
                .simulateMainFrameAutofillServerResponseForTesting(
                        webContents, fieldIds, fieldTypes);
    }

    /**
     * Simulate the server predictions, the first prediction will be set as primary server type.
     */
    public static boolean simulateMainFramePredictionsAutofillServerResponseForTesting(
            WebContents webContents, String[] fieldIds, int[][] fieldTypes) {
        return AutofillProviderTestHelperJni.get()
                .simulateMainFramePredictionsAutofillServerResponseForTesting(
                        webContents, fieldIds, fieldTypes);
    }

    public static void simulateMainFrameAutofillQueryFailedForTesting(WebContents webContents) {
        AutofillProviderTestHelperJni.get().simulateMainFrameAutofillQueryFailedForTesting(
                webContents);
    }

    @NativeMethods
    interface Natives {
        void disableDownloadServerForTesting();
        boolean simulateMainFrameAutofillServerResponseForTesting(
                WebContents webContents, String[] fieldIds, int[] fieldTypes);
        boolean simulateMainFramePredictionsAutofillServerResponseForTesting(
                WebContents webContents, String[] fieldIds, int[][] fieldTypes);
        void simulateMainFrameAutofillQueryFailedForTesting(WebContents webContents);
    }
}
