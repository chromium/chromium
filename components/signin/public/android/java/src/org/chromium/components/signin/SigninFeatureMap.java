// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.CachedFlag;

import java.util.List;

/** Java accessor for base/android/feature_map.h state. */
@JNINamespace("signin")
@NullMarked
public final class SigninFeatureMap extends FeatureMap {
    private static final SigninFeatureMap sInstance = new SigninFeatureMap();

    // Do not instantiate this class.
    private SigninFeatureMap() {}

    public static final CachedFlag sMigrateAccountManagerDelegate =
            new CachedFlag(
                    sInstance,
                    SigninFeatures.MIGRATE_ACCOUNT_MANAGER_DELEGATE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sProfileDiscOnAllPages =
            new CachedFlag(
                    sInstance,
                    SigninFeatures.PROFILE_DISC_ON_ALL_PAGES,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sSigninLevelUpButton =
            new CachedFlag(
                    sInstance,
                    SigninFeatures.SIGNIN_LEVEL_UP_BUTTON,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sSupportForcedSigninPolicy =
            new CachedFlag(
                    sInstance,
                    SigninFeatures.SUPPORT_FORCED_SIGNIN_POLICY,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);
    public static final List<CachedFlag> sCachedFlags =
            List.of(
                    sMigrateAccountManagerDelegate,
                    sProfileDiscOnAllPages,
                    sSigninLevelUpButton,
                    sSupportForcedSigninPolicy);

    /**
     * Returns whether the activityless sign-in is enabled for all entry points.
     *
     * <p>{@link SigninFeatures#ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT} is for migrating rest of
     * the sign-in entry points.
     */
    public boolean isActivitylessSigninAllEntryPointEnabled() {
        return isEnabledInNative(SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT);
    }

    /**
     * @return the singleton SigninFeatureMap.
     */
    public static SigninFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return SigninFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
