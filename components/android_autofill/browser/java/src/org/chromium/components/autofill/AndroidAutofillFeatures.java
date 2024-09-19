// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Features;

/**
 * Java accessor for base/feature_list.h state.
 *
 * <p>This class provides methods to access values of feature flags registered in
 * `kFeaturesExposedToJava` in components/android_autofill/browser/android_autofill_features.cc.
 */
@JNINamespace("autofill::features")
public class AndroidAutofillFeatures extends Features {
    public static final String ANDROID_AUTOFILL_BOTTOM_SHEET_WORKAROUND_NAME =
            "AndroidAutofillBottomSheetWorkaround";
    public static final String ANDROID_AUTOFILL_DEPRECATE_ACCESSIBILITY_API_NAME =
            "AndroidAutofillDeprecateAccessibilityApi";
    public static final String ANDROID_AUTOFILL_DIRECT_FORM_SUBMISSION =
            "AndroidAutofillDirectFormSubmission";
    public static final String ANDROID_AUTOFILL_PREFILL_REQUEST_FOR_CHANGE_PASSWORD_NAME =
            "AndroidAutofillPrefillRequestsForChangePassword";
    public static final AndroidAutofillFeatures ANDROID_AUTOFILL_BOTTOM_SHEET_WORKAROUND =
            new AndroidAutofillFeatures(0, ANDROID_AUTOFILL_BOTTOM_SHEET_WORKAROUND_NAME);
    public static final AndroidAutofillFeatures ANDROID_AUTOFILL_DEPRECATE_ACCESSIBILITY_API =
            new AndroidAutofillFeatures(1, ANDROID_AUTOFILL_DEPRECATE_ACCESSIBILITY_API_NAME);
    private final int mOrdinal;

    private AndroidAutofillFeatures(int ordinal, String name) {
        super(name);
        mOrdinal = ordinal;
    }

    @Override
    protected long getFeaturePointer() {
        return AndroidAutofillFeaturesJni.get().getFeature(mOrdinal);
    }

    @NativeMethods
    interface Natives {
        long getFeature(int ordinal);
    }
}
