// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.ComponentName;
import android.os.Build;
import android.view.autofill.AutofillManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;

/**
 * Java counterpart to AndroidAutofillClient. This class is created by AwContents or ContentView and
 * has a weak reference from native side. Its lifetime matches the duration of the WebView or the
 * WebContents using it.
 */
// TODO(crbug.com/321950502): This class is only an empty hull - consider removing it and the
// references that AwContents and the native side keep to it.
@JNINamespace("android_autofill")
public class AndroidAutofillClient {
    private static final String AWG_COMPONENT_NAME =
            "com.google.android.gms/com.google.android.gms.autofill.service.AutofillService";

    @CalledByNative
    public static AndroidAutofillClient create(long nativeClient) {
        return new AndroidAutofillClient();
    }

    @CalledByNative
    public static boolean allowedForAutofillService() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return false;
        }
        AutofillManager manager =
                ContextUtils.getApplicationContext().getSystemService(AutofillManager.class);
        if (manager == null || !manager.isEnabled()) {
            return false;
        }
        ComponentName componentName = null;
        try {
            componentName = manager.getAutofillServiceComponentName();
        } catch (Exception e) {
        }
        return componentName != null && !AWG_COMPONENT_NAME.equals(componentName.flattenToString());
    }

    private AndroidAutofillClient() {}
}
