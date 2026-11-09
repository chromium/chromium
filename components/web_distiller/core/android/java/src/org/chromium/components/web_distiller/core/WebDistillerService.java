// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.web_distiller.core;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/** Wrapper for native web_distiller::WebDistillerService. */
@JNINamespace("web_distiller::android")
@NullMarked
public class WebDistillerService {

    private final DistilledPagePrefs mDistilledPagePrefs;

    private WebDistillerService(long nativeWebDistillerAndroidServicePtr) {
        mDistilledPagePrefs =
                new DistilledPagePrefs(
                        WebDistillerServiceJni.get()
                                .getDistilledPagePrefsPtr(nativeWebDistillerAndroidServicePtr));
    }

    public DistilledPagePrefs getDistilledPagePrefs() {
        return mDistilledPagePrefs;
    }

    @CalledByNative
    private static WebDistillerService create(long nativeWebDistillerServiceAndroid) {
        ThreadUtils.assertOnUiThread();
        return new WebDistillerService(nativeWebDistillerServiceAndroid);
    }

    @NativeMethods
    interface Natives {
        long getDistilledPagePrefsPtr(long nativeWebDistillerServiceAndroid);
    }
}
