// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for state of Payments feature flags. */
@JNINamespace("payments::android")
public class PaymentFeatureMap extends FeatureMap {
    private static final PaymentFeatureMap sInstance = new PaymentFeatureMap();

    // Do not instantiate this class.
    private PaymentFeatureMap() {
        super();
    }

    /** @return the singleton PaymentFeatureMap. */
    public static PaymentFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return PaymentFeatureMapJni.get().getNativeMap();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
