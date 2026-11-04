// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/**
 * The help class for Autofill Provider test to access the native code.
 *
 * <p>Form parsing in the browser is asynchronous, so a form that the renderer has extracted may not
 * be known to the browser yet. The methods below return false in that case and do nothing; callers
 * should retry them until they return true.
 */
@JNINamespace("autofill")
public class AutofillProviderTestHelper {
    /**
     * Simulates the primary server type only.
     *
     * @return whether the response was simulated, i.e. whether the main frame has parsed a form
     *     that contains a field for each of `fieldIds`.
     */
    public static boolean simulateMainFrameAutofillServerResponseForTesting(
            WebContents webContents, String[] fieldIds, int[] fieldTypes) {
        return AutofillProviderTestHelperJni.get()
                .simulateMainFrameAutofillServerResponseForTesting(
                        webContents, fieldIds, fieldTypes);
    }

    /**
     * Simulates the server predictions, the first prediction will be set as primary server type.
     *
     * @return whether the response was simulated, i.e. whether the main frame has parsed a form
     *     that contains a field for each of `fieldIds`.
     */
    public static boolean simulateMainFramePredictionsAutofillServerResponseForTesting(
            WebContents webContents, String[] fieldIds, int[][] fieldTypes) {
        return AutofillProviderTestHelperJni.get()
                .simulateMainFramePredictionsAutofillServerResponseForTesting(
                        webContents, fieldIds, fieldTypes);
    }

    @NativeMethods
    interface Natives {
        boolean simulateMainFrameAutofillServerResponseForTesting(
                WebContents webContents, String[] fieldIds, int[] fieldTypes);

        boolean simulateMainFramePredictionsAutofillServerResponseForTesting(
                WebContents webContents, String[] fieldIds, int[][] fieldTypes);
    }
}
