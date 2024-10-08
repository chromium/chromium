// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** This class is the java version of the commerce component's feature_utils. */
@JNINamespace("commerce")
public class CommerceFeatureUtils {

    public static boolean isShoppingListEligible(ShoppingService shoppingService) {
        return shoppingService != null
                && CommerceFeatureUtilsJni.get()
                        .isShoppingListEligible(shoppingService.getNativePtr());
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        boolean isShoppingListEligible(long shoppingServicePtr);
    }
}
