// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;

import java.util.HashMap;
import java.util.Map;

/**
 * Java equivalent of C++ survey::SurveyConfig. All member variable are kept package protected, as
 * they should not be access outside of survey code. To add a new SurveyConfig, see
 * //chrome/browser/ui/hats/survey_config.*
 */
@JNINamespace("hats")
public class SurveyConfig {

    private static SurveyConfig sConfigForTesting;

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

    /** Not generated from java. */
    @VisibleForTesting
    SurveyConfig(
            String trigger,
            String triggerId,
            double probability,
            boolean userPrompted,
            String[] psdBitDataFields,
            String[] psdStringDataFields) {
        mTrigger = trigger;
        mTriggerId = triggerId;
        mProbability = probability;
        mUserPrompted = userPrompted;
        mPsdBitDataFields = psdBitDataFields;
        mPsdStringDataFields = psdStringDataFields;
    }

    /**
     * Get the survey config with the associate trigger if the survey config exists and enabled;
     * returns null otherwise.
     *
     * @param trigger The trigger associated with the SurveyConfig.
     * @return SurveyConfig if the survey exists and is enabled.
     */
    @Nullable
    public static SurveyConfig get(String trigger) {
        return get(trigger, "");
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
    @Nullable
    public static SurveyConfig get(String trigger, String suppliedTriggerId) {
        SurveyConfig config;
        if (sConfigForTesting != null && sConfigForTesting.mTrigger.equals(trigger)) {
            config = sConfigForTesting;
        } else {
            config = Holder.getInstance().getSurveyConfig(trigger);
        }
        return getConfigWithSuppliedTriggerIdIfPresent(config, suppliedTriggerId);
    }

    /** Return the dump of input Survey config for debugging purposes. */
    public static String toString(SurveyConfig config) {
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

    /** Clear all the initialized configs. */
    static void clearAll() {
        Holder.getInstance().destroy();
    }

    static SurveyConfig getConfigWithSuppliedTriggerIdIfPresent(
            SurveyConfig config, String suppliedTriggerId) {
        if (config != null && !TextUtils.isEmpty(suppliedTriggerId)) {
            return new SurveyConfig(
                    config.mTrigger,
                    suppliedTriggerId,
                    config.mProbability,
                    config.mUserPrompted,
                    config.mPsdBitDataFields,
                    config.mPsdStringDataFields);
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
            String[] psdStringDataFields) {
        holder.mTriggers.put(
                trigger,
                new SurveyConfig(
                        trigger,
                        triggerId,
                        probability,
                        userPrompted,
                        psdBitDataFields,
                        psdStringDataFields));
    }

    /** Holder that stores all the active surveys for Android. */
    static class Holder {

        private static Holder sInstance;
        private final Map<String, SurveyConfig> mTriggers;
        private long mNativeInstance;

        private Holder() {
            mTriggers = new HashMap<>();
            mNativeInstance = SurveyConfigJni.get().initHolder(this);
        }

        static Holder getInstance() {
            if (sInstance == null) {
                sInstance = new Holder();
            }
            return sInstance;
        }

        SurveyConfig getSurveyConfig(String trigger) {
            return mTriggers.get(trigger);
        }

        void destroy() {
            SurveyConfigJni.get().destroy(mNativeInstance);
            mNativeInstance = 0;
        }
    }

    @NativeMethods
    interface Natives {

        long initHolder(Holder caller);

        void destroy(long nativeSurveyConfigHolder);
    }
}
