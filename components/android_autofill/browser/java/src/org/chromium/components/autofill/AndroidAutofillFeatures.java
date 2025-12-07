// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Features;
import org.chromium.build.annotations.NullMarked;

/**
 * Java accessor for base/feature_list.h state.
 *
 * <p>This class provides methods to access values of feature flags registered in
 * `kFeaturesExposedToJava` in components/android_autofill/browser/android_autofill_features.cc.
 */
@JNINamespace("autofill::features")
@NullMarked
public class AndroidAutofillFeatures extends Features {
    public static final String ANDROID_AUTOFILL_LAZY_FRAMEWORK_WRAPPER_NAME =
            "AndroidAutofillLazyFrameworkWrapper";
    public static final String ANDROID_AUTOFILL_VIRTUAL_VIEW_STRUCTURE_PASSKEY_LONG_PRESS_NAME =
            "AutofillVirtualViewStructureAndroidPasskeyLongPress";
    public static final String ANDROID_AUTOFILL_FORWARD_IFRAME_ORIGIN_NAME =
            "AndroidAutofillForwardIframeOrigin";
    public static final String ANDROID_AUTOFILL_UPDATE_CONTEXT_FOR_WEBCONTENTS_NAME =
            "AndroidAutofillUpdateContextForWebContents";
    public static final String ANDROID_AUTOFILL_IMPROVED_VISIBILITY_DETECTION_NAME =
            "AndroidAutofillImprovedVisibilityDetection";

    public static final AndroidAutofillFeatures ANDROID_AUTOFILL_LAZY_FRAMEWORK_WRAPPER =
            new AndroidAutofillFeatures(0, ANDROID_AUTOFILL_LAZY_FRAMEWORK_WRAPPER_NAME);
    public static final AndroidAutofillFeatures
            ANDROID_AUTOFILL_VIRTUAL_VIEW_STRUCTURE_PASSKEY_LONG_PRESS =
                    new AndroidAutofillFeatures(
                            1, ANDROID_AUTOFILL_VIRTUAL_VIEW_STRUCTURE_PASSKEY_LONG_PRESS_NAME);
    public static final AndroidAutofillFeatures ANDROID_AUTOFILL_FORWARD_IFRAME_ORIGIN =
            new AndroidAutofillFeatures(2, ANDROID_AUTOFILL_FORWARD_IFRAME_ORIGIN_NAME);
    public static final AndroidAutofillFeatures ANDROID_AUTOFILL_UPDATE_CONTEXT_FOR_WEBCONTENTS =
            new AndroidAutofillFeatures(3, ANDROID_AUTOFILL_UPDATE_CONTEXT_FOR_WEBCONTENTS_NAME);
    public static final AndroidAutofillFeatures ANDROID_AUTOFILL_IMPROVED_VISIBILITY_DETECTION =
            new AndroidAutofillFeatures(4, ANDROID_AUTOFILL_IMPROVED_VISIBILITY_DETECTION_NAME);

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
