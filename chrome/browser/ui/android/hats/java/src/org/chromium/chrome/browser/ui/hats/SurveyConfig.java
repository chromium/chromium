// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;

/**
 * Java equivalent of C++ survey::SurveyConfig. All member variable are kept package protected, as
 * they should not be access outside of survey code. To add a new SurveyConfig, see
 * //chrome/browser/ui/hats/survey_config.*
 */
@JNINamespace("hats")
@NullMarked
public class SurveyConfig {

    // LINT.IfChange(RequestedBrowserType)
    @IntDef({RequestedBrowserType.REGULAR, RequestedBrowserType.INCOGNITO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RequestedBrowserType {
        int REGULAR = 0;
        int INCOGNITO = 1;
    }

    // LINT.ThenChange(//chrome/browser/ui/hats/survey_config.h:RequestedBrowserType)

    private static boolean sForceUsingTestingConfig;
    private static @Nullable SurveyConfig sConfigForTesting;

    /** Unique key associate with the config. */
    final String mTrigger;

    /**
     * TriggerId used to download & present the associated survey from HaTS service. The triggerId
     * can be configured in survey_config.cc. TriggerIds can be supplied dynamically as well, in
     * which case they temporarily hide the statically configured triggerId. @see
     * Holder#getSurveyConfig.
     */
    final String mTriggerId;

    /** Probability [0,1] of how likely a chosen user will see the survey. */
    final double mProbability;

    /**
     * Whether the survey will prompt every time because the user has explicitly decided to take the
     * survey e.g. clicking a link.
     */
    final boolean mUserPrompted;

    /** Product Specific Bit Data fields which are sent with the survey response. */
    final String[] mPsdBitDataFields;

    /** Product Specific String Data fields which are sent with the survey response. */
    final String[] mPsdStringDataFields;

    /**
     * Optional parameter which overrides the default survey cooldown period, see {@link
     * SurveyThrottler#MIN_DAYS_BETWEEN_ANY_PROMPT_DISPLAYED}. This value is set only if the survey
     * feature launched for a specific list of users defined by some Google group.
     */
    final Optional<Integer> mCooldownPeriodOverride;

    /** Requested browser type decides where the survey can be shown. */
    final @RequestedBrowserType int mRequestedBrowserType;

    /** Not generated from java. */
    @VisibleForTesting
    SurveyConfig(
            String trigger,
            String triggerId,
            double probability,
            boolean userPrompted,
            String[] psdBitDataFields,
            String[] psdStringDataFields,
            Optional<Integer> cooldownPeriodOverride,
            @RequestedBrowserType int requestedBrowserType) {
        mTrigger = trigger;
        mTriggerId = triggerId;
        mProbability = probability;
        mUserPrompted = userPrompted;
        mPsdBitDataFields = psdBitDataFields;
        mPsdStringDataFields = psdStringDataFields;
        mCooldownPeriodOverride = cooldownPeriodOverride;
        mRequestedBrowserType = requestedBrowserType;
    }

    /**
     * Get the survey config with the associate trigger if the survey config exists and enabled;
     * returns null otherwise.
     *
     * @param trigger The trigger associated with the SurveyConfig.
     * @return SurveyConfig if the survey exists and is enabled.
     */
    public static @Nullable SurveyConfig get(Profile profile, String trigger) {
        return get(profile, trigger, "");
    }

    /**
     * Get the survey config with the associate trigger if the survey config exists and enabled;
     * returns null otherwise. Allows dynamically supplying triggerIds for survey configs.
     *
     * @param trigger The trigger associated with the SurveyConfig.
     * @param suppliedTriggerId if non-empty, creates an otherwise equivalent config with the
     *     triggerId set to suppliedTriggerId
     * @return SurveyConfig if the survey exists and is enabled.
     */
    public static @Nullable SurveyConfig get(
            Profile profile, String trigger, String suppliedTriggerId) {
        SurveyConfig config;
        if (sForceUsingTestingConfig) {
            config = sConfigForTesting;
        } else if (sConfigForTesting != null && sConfigForTesting.mTrigger.equals(trigger)) {
            config = sConfigForTesting;
        } else {
            config = Holder.getInstance(profile).getSurveyConfig(trigger);
        }
        return getConfigWithSuppliedTriggerIdIfPresent(config, suppliedTriggerId);
    }

    /** Return the dump of input Survey config for debugging purposes. */
    public static String toString(@Nullable SurveyConfig config) {
        if (config == null) return "";

        StringBuilder sb = new StringBuilder();
        sb.append("Trigger=")
                .append(config.mTrigger)
                .append(" TriggerId=")
                .append(config.mTriggerId)
                .append(" Probability=")
                .append(config.mProbability)
                .append(" UserPrompted=")
                .append(config.mUserPrompted);

        sb.append(" PsdBitFields=");
        for (String field : config.mPsdBitDataFields) {
            sb.append(field).append(",");
        }

        sb.append(" PsdStringFields=");
        for (String field : config.mPsdStringDataFields) {
            sb.append(field).append(",");
        }

        return sb.toString();
    }

    static void setSurveyConfigForTesting(SurveyConfig config) {
        sConfigForTesting = config;
        ResettersForTesting.register(() -> sConfigForTesting = null);
    }

    static void setSurveyConfigForceUsingTestingConfig(boolean shouldForce) {
        sForceUsingTestingConfig = shouldForce;
        ResettersForTesting.register(() -> sForceUsingTestingConfig = false);
    }

    /** Clear all the initialized configs. */
    static void clearAll() {
        Holder.clearAll();
    }

    static @Nullable SurveyConfig getConfigWithSuppliedTriggerIdIfPresent(
            @Nullable SurveyConfig config, String suppliedTriggerId) {
        if (config != null && !TextUtils.isEmpty(suppliedTriggerId)) {
            return new SurveyConfig(
                    config.mTrigger,
                    suppliedTriggerId,
                    config.mProbability,
                    config.mUserPrompted,
                    config.mPsdBitDataFields,
                    config.mPsdStringDataFields,
                    config.mCooldownPeriodOverride,
                    config.mRequestedBrowserType);
        }
        return config;
    }

    @CalledByNative
    private static void addActiveSurveyConfigToHolder(
            Holder holder,
            String trigger,
            String triggerId,
            double probability,
            boolean userPrompted,
            String[] psdBitDataFields,
            String[] psdStringDataFields,
            int cooldownPeriodOverride,
            @RequestedBrowserType int requestedBrowserType) {
        holder.mTriggers.put(
                trigger,
                new SurveyConfig(
                        trigger,
                        triggerId,
                        probability,
                        userPrompted,
                        psdBitDataFields,
                        psdStringDataFields,
                        cooldownPeriodOverride == 0
                                ? Optional.empty()
                                : Optional.of(cooldownPeriodOverride),
                        requestedBrowserType));
    }

    /** Holder that stores all the active surveys for Android. */
    static class Holder {

        private static @Nullable Holder sInstance;
        private final Map<String, SurveyConfig> mTriggers;
        private long mNativeInstance;

        private Holder(Profile profile) {
            mTriggers = new HashMap<>();
            mNativeInstance = SurveyConfigJni.get().initHolder(this, profile);
        }

        static Holder getInstance(Profile profile) {
            if (sInstance == null) {
                sInstance = new Holder(profile);
            }
            return sInstance;
        }

        static void clearAll() {
            if (sInstance != null) {
                sInstance.destroy();
            }
        }

        @Nullable SurveyConfig getSurveyConfig(String trigger) {
            return mTriggers.get(trigger);
        }

        void destroy() {
            SurveyConfigJni.get().destroy(mNativeInstance);
            mNativeInstance = 0;
        }
    }

    @NativeMethods
    interface Natives {

        long initHolder(Holder caller, Profile profile);

        void destroy(long nativeSurveyConfigHolder);
    }
}
