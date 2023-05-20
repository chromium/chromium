// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureMap;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java accessor for state of Payments feature flags.
 */
@JNINamespace("payments::android")
public class PaymentFeatureMap extends FeatureMap {
    private static final PaymentFeatureMap sInstance = new PaymentFeatureMap();

    // Do not instantiate this class.
    private PaymentFeatureMap() {
        super();
    }

    /**
     * @return the singleton PaymentFeatureMap.
     */
    public static PaymentFeatureMap getInstance() {
        return sInstance;
    }

    @Override
    protected long getNativeMap() {
        return PaymentFeatureMapJni.get().getNativeMap();
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
