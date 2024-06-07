// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.Map;
import java.util.Set;

/** Checks whether hasEnrolledInstrument() can be queried. */
@JNINamespace("payments")
public class HasEnrolledInstrumentQuery {
    /**
     * Checks whether the given hasEnrolledInstrument() query is allowed.
     *
     * @param webContents    The web contents where the query is being performed, cannot be null.
     * @param topLevelOrigin The top level origin using the Payment Request API, cannot be null.
     * @param frameOrigin    The frame origin using the Payment Request API, cannot be null.
     * @param query          The payment method identifiers and payment method specific data, cannot
     *         be null.
     *
     * @return True if the given query for hasEnrolledInstrument() is allowed.
     */
    public static boolean canQuery(
            WebContents webContents,
            String topLevelOrigin,
            String frameOrigin,
            Map<String, PaymentMethodData> query) {
        assert webContents != null;
        assert topLevelOrigin != null;
        assert frameOrigin != null;
        assert query != null;
        return HasEnrolledInstrumentQueryJni.get()
                .canQuery(webContents, topLevelOrigin, frameOrigin, query);
    }

    @CalledByNative
    private static @JniType("std::vector<std::string>") Set<String> getMethodIdentifiers(
            Map<String, PaymentMethodData> query) {
        return query.keySet();
    }

    @CalledByNative
    private static @JniType("std::string") String getStringifiedMethodData(
            Map<String, PaymentMethodData> query, String methodIdentifier) {
        assert query.containsKey(methodIdentifier);
        return query.get(methodIdentifier).stringifiedData;
    }

    private HasEnrolledInstrumentQuery() {} // Do not instantiate.

    @NativeMethods
    interface Natives {
        boolean canQuery(
                WebContents webContents,
                String topLevelOrigin,
                String frameOrigin,
                Map<String, PaymentMethodData> query);
    }
}
