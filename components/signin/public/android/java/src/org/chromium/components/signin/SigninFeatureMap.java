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

    public static final CachedFlag sFullscreenSigninMediatorStateMachine =
            new CachedFlag(
                    sInstance,
                    SigninFeatures.FULLSCREEN_SIGNIN_MEDIATOR_STATE_MACHINE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ false);
    public static final CachedFlag sMigrateAccountManagerDelegate =
            new CachedFlag(
                    sInstance,
                    SigninFeatures.MIGRATE_ACCOUNT_MANAGER_DELEGATE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sSigninLevelUpButton =
            new CachedFlag(
                    sInstance,
                    SigninFeatures.SIGNIN_LEVEL_UP_BUTTON,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);
    public static final List<CachedFlag> sCachedFlags =
            List.of(
                    sFullscreenSigninMediatorStateMachine,
                    sMigrateAccountManagerDelegate,
                    sSigninLevelUpButton);

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
     * Returns whether the AI tier ring is enabled on the settings account surfaces: the Main
     * Settings account row and the Account page card. Their layouts and avatar sizes depend on it.
     *
     * <p>Needs both {@link SigninFeatures#ENABLE_AI_SUBSCRIPTION_AVATAR_RING} and {@link
     * SigninFeatures#AI_SUBSCRIPTION_AVATAR_RING_FOLLOWUP}.
     */
    public boolean isAiTierRingInSettingsEnabled() {
        return isEnabledInNative(SigninFeatures.ENABLE_AI_SUBSCRIPTION_AVATAR_RING)
                && isEnabledInNative(SigninFeatures.AI_SUBSCRIPTION_AVATAR_RING_FOLLOWUP);
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
