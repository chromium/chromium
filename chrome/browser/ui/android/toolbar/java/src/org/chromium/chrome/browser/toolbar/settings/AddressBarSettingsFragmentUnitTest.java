// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link AddressBarSettingsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AddressBarSettingsFragmentUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private SharedPreferencesManager mSharedPreferencesManager;
    private AddressBarSettingsFragment mSettings;
    private RadioButtonWithDescription mTopButton;
    private RadioButtonWithDescription mBottomButton;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED);
    }

    private void onActivity(Activity activity) {
        mActivity = (TestActivity) activity;
    }

    private void launchFragment() {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        mSettings =
                (AddressBarSettingsFragment)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        AddressBarSettingsFragment.class.getClassLoader(),
                                        AddressBarSettingsFragment.class.getName());

        fragmentManager.beginTransaction().replace(android.R.id.content, mSettings).commit();
        mActivityScenarioRule.getScenario().moveToState(State.STARTED);

        assertEquals(
                mActivity.getString(R.string.address_bar_settings), mSettings.getPageTitle().get());

        AddressBarPreference addressBarPreference =
                (AddressBarPreference)
                        mSettings.findPreference(
                                AddressBarSettingsFragment.PREF_ADDRESS_BAR_PREFERENCE);
        mTopButton = (RadioButtonWithDescription) addressBarPreference.getTopRadioButton();
        mBottomButton = (RadioButtonWithDescription) addressBarPreference.getBottomRadioButton();
    }

    @Test
    @SmallTest
    public void testTopAndThenSelectBottom() {
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);

        launchFragment();
        assertTrue(mTopButton.isChecked());
        assertFalse(mBottomButton.isChecked());

        mBottomButton.performClick();

        assertFalse(mTopButton.isChecked());
        assertTrue(mBottomButton.isChecked());
        assertFalse(
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true));
    }

    @Test
    @SmallTest
    public void testBottomAndThenSelectTop() {
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false);

        launchFragment();
        assertFalse(mTopButton.isChecked());
        assertTrue(mBottomButton.isChecked());

        mTopButton.performClick();

        assertTrue(mTopButton.isChecked());
        assertFalse(mBottomButton.isChecked());
        assertTrue(
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false));
    }
}
