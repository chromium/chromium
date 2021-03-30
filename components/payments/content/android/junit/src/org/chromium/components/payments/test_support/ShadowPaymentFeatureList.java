// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.test_support;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.components.payments.PaymentFeatureList;

import java.util.HashMap;
import java.util.Map;

/** The shadow of PaymentFeatureList. */
@Implements(PaymentFeatureList.class)
public class ShadowPaymentFeatureList {
    private static final Map<String, Boolean> sFeatureStatuses = new HashMap<>();

    @Resetter
    public static void reset() {
        sFeatureStatuses.clear();
    }

    @Implementation
    public static boolean isEnabled(String featureName) {
        assert sFeatureStatuses.containsKey(featureName) : "The feature state has yet been set.";
        return sFeatureStatuses.get(featureName);
    }

    /**
     * Set the given feature to be enabled.
     * @param featureName The name of the feature.
     * @param enabled Whether to enable the feature.
     */
    public static void setFeatureEnabled(String featureName, boolean enabled) {
        sFeatureStatuses.put(featureName, enabled);
    }
}