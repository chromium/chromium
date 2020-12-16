// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.annotation.TargetApi;
import android.os.Build;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.annotations.VerifiesOnO;
import org.chromium.content_public.browser.WebContents;

/**
 * The help class for Autofill Provider test to access the native code.
 */
@VerifiesOnO
@TargetApi(Build.VERSION_CODES.O)
@JNINamespace("autofill")
public class AutofillProviderTestHelper {
    public static boolean simulateMainFrameAutofillServerResponseForTesting(
            WebContents webContents, String[] fieldIds, int[] fieldTypes) {
        return AutofillProviderTestHelperJni.get()
                .simulateMainFrameAutofillServerResponseForTesting(
                        webContents, fieldIds, fieldTypes);
    }

    public static void simulateMainFrameAutofillQueryFailedForTesting(WebContents webContents) {
        AutofillProviderTestHelperJni.get().simulateMainFrameAutofillQueryFailedForTesting(
                webContents);
    }

    @NativeMethods
    interface Natives {
        boolean simulateMainFrameAutofillServerResponseForTesting(
                WebContents webContents, String[] fieldIds, int[] fieldTypes);
        void simulateMainFrameAutofillQueryFailedForTesting(WebContents webContents);
    }
}
