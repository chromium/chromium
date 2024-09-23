// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.hamcrest.Matchers;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Facilitates testing of Fragments/Settings using the BlankUiTestActivity */
public class BlankUiTestActivitySettingsTestRule extends BaseActivityTestRule<BlankUiTestActivity> {
    private PreferenceFragmentCompat mPreferenceFragment;
    private PreferenceScreen mPreferenceScreen;

    public BlankUiTestActivitySettingsTestRule() {
        super(BlankUiTestActivity.class);
    }

    /**
     * Ensures the activity is launched, and creates an instance of the preference class specified
     * and attaches it.
     * @param preferenceClass The preference type to be created.
     */
    public void launchPreference(Class<? extends PreferenceFragmentCompat> preferenceClass) {
        launchPreference(preferenceClass, null);
    }

    /**
     * Ensures the activity is launched, and creates an instance of the preference class specified
     * and attaches it.
     * @param preferenceClass The preference type to be created.
     * @param fragmentArgs Optional arguments to be set on the fragment.
     */
    public void launchPreference(
            Class<? extends PreferenceFragmentCompat> preferenceClass,
            @Nullable Bundle fragmentArgs) {
        launchPreference(preferenceClass, fragmentArgs, null);
    }

    /**
     * Ensures the activity is launched, and creates an instance of the preference class specified
     * and attaches it.
     * @param preferenceClass The preference type to be created.
     * @param fragmentArgs Optional arguments to be set on the fragment.
     * @param fragmentInitCallback An initialization callback to be called after creating the
     *                             Fragment and before attaching it to the activity.
     */
    public void launchPreference(
            Class<? extends PreferenceFragmentCompat> preferenceClass,
            @Nullable Bundle fragmentArgs,
            @Nullable Callback<PreferenceFragmentCompat> fragmentInitCallback) {
        if (getActivity() == null) launchActivity(null);

        PreferenceFragmentCompat preference =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PreferenceFragmentCompat fragment =
                                    (PreferenceFragmentCompat)
                                            getActivity()
                                                    .getSupportFragmentManager()
                                                    .getFragmentFactory()
                                                    .instantiate(
                                                            preferenceClass.getClassLoader(),
                                                            preferenceClass.getName());
                            if (fragmentArgs != null) {
                                fragment.setArguments(fragmentArgs);
                            }
                            if (fragmentInitCallback != null) {
                                fragmentInitCallback.onResult(fragment);
                            }
                            return fragment;
                        });
        launchPreference(preference);
    }

    /**
     * Ensures the activity is launched and attaches the given preference.
     * @param preference The preference to be attached.
     */
    public void launchPreference(PreferenceFragmentCompat preference) {
        if (getActivity() == null) launchActivity(null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPreferenceFragment = preference;
                    getActivity()
                            .getSupportFragmentManager()
                            .beginTransaction()
                            .replace(android.R.id.content, mPreferenceFragment)
                            .commit();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mPreferenceFragment.getPreferenceManager(), Matchers.notNullValue());
                    Criteria.checkThat(
                            mPreferenceFragment.getPreferenceScreen(), Matchers.notNullValue());
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPreferenceScreen = mPreferenceFragment.getPreferenceScreen();
                });
    }

    /** @return The preference fragment attached in {@link #launchPreference}. */
    public PreferenceFragmentCompat getPreferenceFragment() {
        return mPreferenceFragment;
    }

    /** @return The preference screen associated with the attached preference. */
    public PreferenceScreen getPreferenceScreen() {
        return mPreferenceScreen;
    }
}
