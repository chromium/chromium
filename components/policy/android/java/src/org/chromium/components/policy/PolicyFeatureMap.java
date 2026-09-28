// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.CachedFlag;

import java.util.List;

/** Java accessor for base/android/feature_map.h state. */
@JNINamespace("policy")
@NullMarked
public final class PolicyFeatureMap extends FeatureMap {
    private static final PolicyFeatureMap sInstance = new PolicyFeatureMap();

    // Do not instantiate this class.
    private PolicyFeatureMap() {}

    public static final CachedFlag sAndroidUseAdminsForEnterpriseInfo =
            new CachedFlag(
                    sInstance,
                    PolicyFeatures.ANDROID_USE_ADMINS_FOR_ENTERPRISE_INFO,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);

    public static final CachedFlag sUseRestrictionsManagerInAppRestrictionsProvider =
            new CachedFlag(
                    sInstance,
                    PolicyFeatures.USE_RESTRICTIONS_MANAGER_IN_APP_RESTRICTIONS_PROVIDER,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);

    public static final List<CachedFlag> sCachedFlags =
            List.of(
                    sAndroidUseAdminsForEnterpriseInfo,
                    sUseRestrictionsManagerInAppRestrictionsProvider);

    public static PolicyFeatureMap getInstance() {
        return sInstance;
    }

    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return PolicyFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
