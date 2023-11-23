// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import java.util.Set;

/** Error messages for web payment.  */
@JNINamespace("payments::android")
public class ErrorMessageUtil {
    /**
     * Returns the "payment method not supported" message.
     * @param methods The payment methods that are not supported.
     * @return The web-developer facing error message.
     */
    public static String getNotSupportedErrorMessage(Set<String> methods) {
        return ErrorMessageUtilJni.get()
                .getNotSupportedErrorMessage(methods.toArray(new String[methods.size()]));
    }

    /**
     * The interface implemented by the automatically generated JNI bindings class
     * ErrorMessageUtilJni.
     */
    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        String getNotSupportedErrorMessage(String[] methods);
    }
}
