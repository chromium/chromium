// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import androidx.annotation.LayoutRes;
import androidx.preference.Preference;

import org.chromium.components.browser_ui.settings.test.R;

/** Instances of {@link ManagedPreferenceDelegate} used by tests. */
public class ManagedPreferenceTestDelegates {
    /** A configurable {@link ManagedPreferenceDelegate} for tests. */
    public static class TestManagedPreferenceDelegate implements ManagedPreferenceDelegate {
        private boolean mIsControlledByPolicy;
        private boolean mIsControlledByCustodian;
        private Boolean mIsRecommendation;
        private boolean mHasMultipleCustodians;
        private @LayoutRes int mLayoutRes = R.layout.chrome_managed_preference;

        public TestManagedPreferenceDelegate setIsControlledByPolicy(boolean isControlledByPolicy) {
            mIsControlledByPolicy = isControlledByPolicy;
            return this;
        }

        public TestManagedPreferenceDelegate setIsControlledByCustodian(
                boolean isControlledByCustodian) {
            mIsControlledByCustodian = isControlledByCustodian;
            return this;
        }

        public TestManagedPreferenceDelegate setIsRecommendation(Boolean isRecommendation) {
            mIsRecommendation = isRecommendation;
            return this;
        }

        public TestManagedPreferenceDelegate setHasMultipleCustodians(
                boolean hasMultipleCustodians) {
            mHasMultipleCustodians = hasMultipleCustodians;
            return this;
        }

        public TestManagedPreferenceDelegate setLayoutRes(@LayoutRes int layoutRes) {
            mLayoutRes = layoutRes;
            return this;
        }

        @Override
        public boolean isPreferenceControlledByPolicy(Preference preference) {
            return mIsControlledByPolicy;
        }

        @Override
        public boolean isPreferenceControlledByCustodian(Preference preference) {
            return mIsControlledByCustodian;
        }

        @Override
        public Boolean isPreferenceRecommendation(Preference preference) {
            return mIsRecommendation;
        }

        @Override
        public boolean doesProfileHaveMultipleCustodians() {
            return mHasMultipleCustodians;
        }

        @Override
        public @LayoutRes int defaultPreferenceLayoutResource() {
            return mLayoutRes;
        }
    }

    public static final ManagedPreferenceDelegate UNMANAGED_DELEGATE =
            new TestManagedPreferenceDelegate()
                    .setIsControlledByPolicy(false)
                    .setIsControlledByCustodian(false)
                    .setIsRecommendation(null)
                    .setLayoutRes(0);

    public static final ManagedPreferenceDelegate POLICY_DELEGATE =
            new TestManagedPreferenceDelegate().setIsControlledByPolicy(true);

    public static final ManagedPreferenceDelegate SINGLE_CUSTODIAN_DELEGATE =
            new TestManagedPreferenceDelegate().setIsControlledByCustodian(true);

    public static final ManagedPreferenceDelegate MULTI_CUSTODIAN_DELEGATE =
            new TestManagedPreferenceDelegate()
                    .setIsControlledByCustodian(true)
                    .setHasMultipleCustodians(true);

    public static final ManagedPreferenceDelegate RECOMMENDED_DELEGATE_FOLLOWING =
            new TestManagedPreferenceDelegate().setIsRecommendation(true);

    public static final ManagedPreferenceDelegate RECOMMENDED_DELEGATE_OVERRIDDEN =
            new TestManagedPreferenceDelegate().setIsRecommendation(false);

    public static final ManagedPreferenceDelegate POLICY_AND_RECOMMENDED_DELEGATE =
            new TestManagedPreferenceDelegate()
                    .setIsControlledByPolicy(true)
                    .setIsRecommendation(true);

    public static final ManagedPreferenceDelegate CUSTODIAN_AND_RECOMMENDED_DELEGATE =
            new TestManagedPreferenceDelegate()
                    .setIsControlledByCustodian(true)
                    .setIsRecommendation(true);

    public static final ManagedPreferenceDelegate ALL_MANAGED_DELEGATE =
            new TestManagedPreferenceDelegate()
                    .setIsControlledByPolicy(true)
                    .setIsControlledByCustodian(true)
                    .setIsRecommendation(true);
}
