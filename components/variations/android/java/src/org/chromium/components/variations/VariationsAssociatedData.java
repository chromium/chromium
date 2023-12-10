// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import java.util.HashMap;

/** Wrapper for variations. */
@JNINamespace("variations::android")
public final class VariationsAssociatedData {

    private VariationsAssociatedData() {}

    /**
     * @param trialName The name of the trial to get the param value for.
     * @param paramName The name of the param for which to get the value.
     * @return The parameter value. Empty string if the field trial does not exist or the specified
     *     parameter does not exist.
     */
    public static String getVariationParamValue(String trialName, String paramName) {
        return VariationsAssociatedDataJni.get().getVariationParamValue(trialName, paramName);
    }

    public static HashMap<String, String> getFeedbackMap() {
        HashMap<String, String> map = new HashMap<String, String>();
        map.put("Chrome Variations", VariationsAssociatedDataJni.get().getFeedbackVariations());
        return map;
    }

    /**
     * Returns the list of Google App variations from active finch field trials.
     * @return A space separated list of ids with leading and trailing space.
     * For example, " 123 456 ".
     * IMPORTANT: This string is only approved for integrations with the Android
     * Google App and must receive a privacy review before extending to other apps.
     */
    public static String getGoogleAppVariations() {
        String variations = VariationsAssociatedDataJni.get().getGoogleAppVariations();
        return variations;
    }

    @NativeMethods
    interface Natives {
        String getVariationParamValue(String trialName, String paramName);

        String getFeedbackVariations();

        String getGoogleAppVariations();
    }
}
