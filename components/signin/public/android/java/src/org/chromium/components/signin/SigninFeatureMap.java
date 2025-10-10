// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import androidx.annotation.IntDef;

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
    public static final List<CachedFlag> sCachedFlags = List.of(sMigrateAccountManagerDelegate);

    /** Layout type for the sign-in promo. */
    @IntDef({
        SeamlessSigninPromoType.NON_SEAMLESS,
        SeamlessSigninPromoType.COMPACT,
        SeamlessSigninPromoType.TWO_BUTTONS
    })
    public @interface SeamlessSigninPromoType {
        int NON_SEAMLESS = 0;
        int COMPACT = 1;
        int TWO_BUTTONS = 2;
    }

    /** Returns the currently enabled sign-in promo type. */
    public @SeamlessSigninPromoType int getSeamlessSigninPromoType() {
        String promoType =
                SigninFeatureMap.getInstance()
                        .getFieldTrialParamByFeature(
                                SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
                                "seamless-signin-promo-type");
        switch (promoType) {
            case "compact":
                return SeamlessSigninPromoType.COMPACT;
            case "twoButtons":
                return SeamlessSigninPromoType.TWO_BUTTONS;
            default:
                return SeamlessSigninPromoType.NON_SEAMLESS;
        }
    }

    /** Strings for the sign-in promo. */
    @IntDef({
        SeamlessSigninStringType.NON_SEAMLESS,
        SeamlessSigninStringType.CONTINUE_BUTTON,
        SeamlessSigninStringType.SIGNIN_BUTTON
    })
    public @interface SeamlessSigninStringType {
        int NON_SEAMLESS = 0;
        int CONTINUE_BUTTON = 1;
        int SIGNIN_BUTTON = 2;
    }

    /** Returns the set of strings that is currently enabled for the seamless sign-in experiment. */
    public @SeamlessSigninStringType int getSeamlessSigninStringType() {
        String stringType =
                SigninFeatureMap.getInstance()
                        .getFieldTrialParamByFeature(
                                SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
                                "seamless-signin-string-type");
        switch (stringType) {
            case "continueButton":
                return SeamlessSigninStringType.CONTINUE_BUTTON;
            case "signinButton":
                return SeamlessSigninStringType.SIGNIN_BUTTON;
            default:
                return SeamlessSigninStringType.NON_SEAMLESS;
        }
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
