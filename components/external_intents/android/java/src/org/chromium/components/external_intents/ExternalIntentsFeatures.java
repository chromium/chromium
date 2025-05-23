// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Features;
import org.chromium.build.annotations.NullMarked;

/**
 * Java accessor for base/feature_list.h state.
 *
 * <p>This class provides methods to access values of feature flags registered in
 * |kFeaturesExposedToJava| in components/external_intents/android/external_intents_features.cc.
 */
@JNINamespace("external_intents")
@NullMarked
public class ExternalIntentsFeatures {
    public static final String EXTERNAL_NAVIGATION_DEBUG_LOGS_NAME = "ExternalNavigationDebugLogs";
    public static final String BLOCK_INTENTS_TO_SELF_NAME = "BlockIntentsToSelf";
    public static final String NAVIGATION_CAPTURE_REFACTOR_ANDROID_NAME =
            "NavigationCaptureRefactorAndroid";
    public static final String AUXILIARY_NAVIGATION_STAYS_IN_BROWSER_NAME =
            "AuxiliaryNavigationStaysInBrowser";
    public static final String REPARENT_TOP_LEVEL_NAVIGATION_FROM_PWA_NAME =
            "ReparentTopLevelNavigationFromPWA";
    public static final String REPARENT_AUXILIARY_NAVIGATION_FROM_PWA_NAME =
            "ReparentAuxiliaryNavigationFromPWA";

    public static final ExternalIntentsFeature EXTERNAL_NAVIGATION_DEBUG_LOGS =
            new ExternalIntentsFeature(0, EXTERNAL_NAVIGATION_DEBUG_LOGS_NAME);

    public static final ExternalIntentsFeature BLOCK_INTENTS_TO_SELF =
            new ExternalIntentsFeature(1, BLOCK_INTENTS_TO_SELF_NAME);

    public static final ExternalIntentsFeature NAVIGATION_CAPTURE_REFACTOR_ANDROID =
            new ExternalIntentsFeature(2, NAVIGATION_CAPTURE_REFACTOR_ANDROID_NAME);

    public static final AuxiliaryNavigationStaysInBrowserFeature
            AUXILIARY_NAVIGATION_STAYS_IN_BROWSER =
                    new AuxiliaryNavigationStaysInBrowserFeature(
                            3, AUXILIARY_NAVIGATION_STAYS_IN_BROWSER_NAME);

    public static final ExternalIntentsFeature REPARENT_TOP_LEVEL_NAVIGATION_FROM_PWA =
            new ExternalIntentsFeature(4, REPARENT_TOP_LEVEL_NAVIGATION_FROM_PWA_NAME);

    public static final ExternalIntentsFeature REPARENT_AUXILIARY_NAVIGATION_FROM_PWA =
            new ExternalIntentsFeature(5, REPARENT_AUXILIARY_NAVIGATION_FROM_PWA_NAME);

    public static class ExternalIntentsFeature extends Features {
        private final int mOrdinal;

        private ExternalIntentsFeature(int ordinal, String name) {
            super(name);
            mOrdinal = ordinal;
        }

        @Override
        protected long getFeaturePointer() {
            return ExternalIntentsFeaturesJni.get().getFeature(mOrdinal);
        }
    }

    @NativeMethods
    interface Natives {
        long getFeature(int ordinal);
    }

    public static class AuxiliaryNavigationStaysInBrowserFeature extends ExternalIntentsFeature {
        private static final String PARAM_NAME = "auxiliary_navigation_stays_in_browser";
        private static final String DESKTOP_WM_FIELD = "desktop_wm";
        private static final String ALL_WM_FIELD = "all_wm";

        private AuxiliaryNavigationStaysInBrowserFeature(int ordinal, String name) {
            super(ordinal, name);
        }

        public boolean isEnabled(boolean isInDesktopWindowingMode) {
            if (!isEnabled()) {
                return false;
            }

            String featureString = getFieldTrialParamByFeatureAsString(PARAM_NAME);

            // The feature is supposed to work for desktop windowing only.
            if (featureString.equals(DESKTOP_WM_FIELD) && isInDesktopWindowingMode) {
                return true;
            }

            // The feature is supposed to work independently of windowing mode.
            if (featureString.equals(ALL_WM_FIELD)) {
                return true;
            }

            // Enabled for testing. Also corresponds to the "Enabled" option in chrome://flags.
            if (featureString.isEmpty()) {
                return true;
            }

            // Feature is enabled but a wrong param was specified.
            return false;
        }
    }
}
