// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import androidx.annotation.LayoutRes;
import androidx.preference.Preference;

import org.chromium.components.browser_ui.settings.test.R;

/** Instances of {@link ManagedPreferenceDelegate} used by tests. */
public class ManagedPreferenceTestDelegates {
    public static final ManagedPreferenceDelegate UNMANAGED_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return false;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return false;
                }

                @Override
                public @LayoutRes int defaultPreferenceLayoutResource() {
                    return 0;
                }
            };

    public static final ManagedPreferenceDelegate POLICY_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return true;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return false;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return false;
                }

                @Override
                public @LayoutRes int defaultPreferenceLayoutResource() {
                    return R.layout.chrome_managed_preference;
                }
            };

    public static final ManagedPreferenceDelegate SINGLE_CUSTODIAN_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return true;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return false;
                }

                @Override
                public @LayoutRes int defaultPreferenceLayoutResource() {
                    return R.layout.chrome_managed_preference;
                }
            };

    public static final ManagedPreferenceDelegate MULTI_CUSTODIAN_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return true;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return true;
                }

                @Override
                public @LayoutRes int defaultPreferenceLayoutResource() {
                    return R.layout.chrome_managed_preference;
                }
            };
}
