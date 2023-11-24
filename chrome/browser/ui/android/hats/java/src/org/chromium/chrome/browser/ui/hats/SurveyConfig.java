// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;

import java.util.HashMap;
import java.util.Map;

/**
 * Java equivalent of C++ survey::SurveyConfig. All member variable are kept package protected,
 * as they should not be access outside of survey code.
 * To add a new SurveyConfig, see //chrome/browser/ui/hats/survey_config.*
 */
@JNINamespace("hats")
public class SurveyConfig {
    private static SurveyConfig sConfigForTesting;

    /** Unique key associate with the config. */
    final String mTrigger;

    /** TriggerId used to download & present the associated survey from HaTS service. */
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
     * Get the survey config with the associate trigger if the survey config exists and enabled;
     * returns null otherwise.
     *
     * @param trigger The trigger associated with the SurveyConfig.
     * @return SurveyConfig if the survey exists and is enabled.
     */
    @Nullable
    public static SurveyConfig get(String trigger) {
        if (sConfigForTesting != null && sConfigForTesting.mTrigger.equals(trigger)) {
            return sConfigForTesting;
        }
        return Holder.getInstance().getSurveyConfig(trigger);
    }

    static void setSurveyConfigForTesting(SurveyConfig config) {
        sConfigForTesting = config;
        ResettersForTesting.register(() -> sConfigForTesting = null);
    }

    /** Clear all the initialized configs. */
    static void clearAll() {
        Holder.getInstance().destroy();
    }

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
        private long mNativeInstance;
        private final Map<String, SurveyConfig> mTriggers;

        static Holder getInstance() {
            if (sInstance == null) {
                sInstance = new Holder();
            }
            return sInstance;
        }

        private Holder() {
            mTriggers = new HashMap<>();
            mNativeInstance = SurveyConfigJni.get().initHolder(this);
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
