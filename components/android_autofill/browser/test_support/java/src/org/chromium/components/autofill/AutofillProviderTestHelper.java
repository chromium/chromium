// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** The help class for Autofill Provider test to access the native code. */
@JNINamespace("autofill")
public class AutofillProviderTestHelper {
    /**
     * Disable crowdsourcing for testing to avoid that the server response affects the integration
     * tests. Must be called before WebContents is created.
     */
    public static void disableCrowdsourcingForTesting() {
        AutofillProviderTestHelperJni.get().disableCrowdsourcingForTesting();
    }

    /** Simulate the primary server type only. */
    public static boolean simulateMainFrameAutofillServerResponseForTesting(
            WebContents webContents, String[] fieldIds, int[] fieldTypes) {
        return AutofillProviderTestHelperJni.get()
                .simulateMainFrameAutofillServerResponseForTesting(
                        webContents, fieldIds, fieldTypes);
    }

    /** Simulate the server predictions, the first prediction will be set as primary server type. */
    public static boolean simulateMainFramePredictionsAutofillServerResponseForTesting(
            WebContents webContents, String[] fieldIds, int[][] fieldTypes) {
        return AutofillProviderTestHelperJni.get()
                .simulateMainFramePredictionsAutofillServerResponseForTesting(
                        webContents, fieldIds, fieldTypes);
    }

    @NativeMethods
    interface Natives {
        void disableCrowdsourcingForTesting();

        boolean simulateMainFrameAutofillServerResponseForTesting(
                WebContents webContents, String[] fieldIds, int[] fieldTypes);

        boolean simulateMainFramePredictionsAutofillServerResponseForTesting(
                WebContents webContents, String[] fieldIds, int[][] fieldTypes);
    }
}
