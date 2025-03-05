// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.widget.ImageView;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.preference.Preference;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
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
    private Preference mAddressBarTitle;
    private ImageView mToolbarPositionImage;

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
                AddressBarSettingsFragment.getTitle(mActivity), mSettings.getPageTitle().get());

        AddressBarPreference addressBarPreference =
                (AddressBarPreference)
                        mSettings.findPreference(
                                AddressBarSettingsFragment.PREF_ADDRESS_BAR_PREFERENCE);
        mTopButton = (RadioButtonWithDescription) addressBarPreference.getTopRadioButton();
        mBottomButton = (RadioButtonWithDescription) addressBarPreference.getBottomRadioButton();
        mAddressBarTitle =
                mSettings.findPreference(AddressBarSettingsFragment.PREF_ADDRESS_BAR_TITLE);

        AddressBarHeaderPreference addressBarHeaderPreference =
                (AddressBarHeaderPreference)
                        mSettings.findPreference(
                                AddressBarSettingsFragment.PREF_ADDRESS_BAR_HEADER);
        mToolbarPositionImage = addressBarHeaderPreference.getToolbarPositionImage();
    }

    @Test
    @SmallTest
    public void testTopAndThenSelectBottom() {
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);

        launchFragment();
        assertEquals(
                mActivity.getString(R.string.address_bar_settings_description),
                mAddressBarTitle.getSummary());
        assertTrue(mTopButton.isChecked());
        assertFalse(mBottomButton.isChecked());
        assertTrue(mToolbarPositionImage.isSelected());
        assertEquals(
                mActivity.getString(R.string.address_bar_settings_currently_on_top),
                mToolbarPositionImage.getContentDescription());

        mBottomButton.performClick();

        assertFalse(mTopButton.isChecked());
        assertTrue(mBottomButton.isChecked());
        assertFalse(
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true));
        assertFalse(mToolbarPositionImage.isSelected());
        assertEquals(
                mActivity.getString(R.string.address_bar_settings_currently_on_bottom),
                mToolbarPositionImage.getContentDescription());
    }

    @Test
    @SmallTest
    public void testBottomAndThenSelectTop() {
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false);

        launchFragment();
        assertFalse(mTopButton.isChecked());
        assertTrue(mBottomButton.isChecked());
        assertFalse(mToolbarPositionImage.isSelected());

        mTopButton.performClick();

        assertTrue(mTopButton.isChecked());
        assertFalse(mBottomButton.isChecked());
        assertTrue(
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false));
        assertTrue(mToolbarPositionImage.isSelected());
    }

    @Test
    @SmallTest
    @Config(sdk = android.os.Build.VERSION_CODES.R)
    public void testFoldable() {
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE, true);
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);

        launchFragment();
        assertEquals(
                mActivity.getString(R.string.address_bar_settings_description_foldable),
                mAddressBarTitle.getSummary());
    }
}
