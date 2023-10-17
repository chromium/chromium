// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.chromium.base.Features;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java accessor for base/feature_list.h state.
 *
 * This class provides methods to access values of feature flags registered in
 * `kFeaturesExposedToJava` in components/android_autofill/browser/android_autofill_features.cc.
 *
 */
@JNINamespace("autofill::features")
public class AndroidAutofillFeatures extends Features {
    public static final String ANDROID_AUTOFILL_FORM_SUBMISSION_CHECK_BY_ID_NAME =
            "AndroidAutofillFormSubmissionCheckById";
    public static final String ANDROID_AUTOFILL_SUPPORT_VISIBILITY_CHANGES_NAME =
            "AndroidAutofillSupportVisibilityChanges";
    public static final String ANDROID_AUTOFILL_VIEW_STRUCTURE_WITH_FORM_HIERARCHY_LAYER_NAME =
            "AndroidAutofillViewStructureWithFormHierarchyLayer";

    public static final AndroidAutofillFeatures ANDROID_AUTOFILL_FORM_SUBMISSION_CHECK_BY_ID =
            new AndroidAutofillFeatures(0, ANDROID_AUTOFILL_FORM_SUBMISSION_CHECK_BY_ID_NAME);
    public static final AndroidAutofillFeatures ANDROID_AUTOFILL_SUPPORT_VISIBILITY_CHANGES =
            new AndroidAutofillFeatures(1, ANDROID_AUTOFILL_SUPPORT_VISIBILITY_CHANGES_NAME);
    public static final AndroidAutofillFeatures
            ANDROID_AUTOFILL_VIEW_STRUCTURE_WITH_FORM_HIERARCHY_LAYER = new AndroidAutofillFeatures(
                    2, ANDROID_AUTOFILL_VIEW_STRUCTURE_WITH_FORM_HIERARCHY_LAYER_NAME);

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
