// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/** Wrapper for native dom_distiller::DomDistillerService. */
@JNINamespace("dom_distiller::android")
@NullMarked
public class DomDistillerService {

    private final DistilledPagePrefs mDistilledPagePrefs;

    private DomDistillerService(long nativeDomDistillerAndroidServicePtr) {
        mDistilledPagePrefs =
                new DistilledPagePrefs(
                        DomDistillerServiceJni.get()
                                .getDistilledPagePrefsPtr(nativeDomDistillerAndroidServicePtr));
    }

    public DistilledPagePrefs getDistilledPagePrefs() {
        return mDistilledPagePrefs;
    }

    @CalledByNative
    private static DomDistillerService create(long nativeDomDistillerServiceAndroid) {
        ThreadUtils.assertOnUiThread();
        return new DomDistillerService(nativeDomDistillerServiceAndroid);
    }

    @NativeMethods
    interface Natives {
        long getDistilledPagePrefsPtr(long nativeDomDistillerServiceAndroid);
    }
}
