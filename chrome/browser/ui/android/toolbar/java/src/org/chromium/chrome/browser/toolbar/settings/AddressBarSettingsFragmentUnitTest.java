// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.prefs.LocalStatePrefsJni;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.ToolbarPositionAndSource;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment.HighlightedOption;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link AddressBarSettingsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AddressBarSettingsFragmentUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

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
    private @Mock LocalStatePrefs.Natives mLocalStatePrefsNatives;
    private @Mock PrefService mLocalPrefService;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        LocalStatePrefs.setNativePrefsLoadedForTesting(true);
        LocalStatePrefsJni.setInstanceForTesting(mLocalStatePrefsNatives);
        when(mLocalStatePrefsNatives.getPrefService()).thenReturn(mLocalPrefService);

        AtomicReference<@Nullable Boolean> localPrefValue = new AtomicReference<>();
        doAnswer(
                        invocation -> {
                            localPrefValue.set(invocation.getArgument(1));
                            return null;
                        })
                .when(mLocalPrefService)
                .setBoolean(eq(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION), anyBoolean());
        when(mLocalPrefService.hasPrefPath(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION))
                .thenAnswer(invocation -> localPrefValue.get() != null);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION))
                .thenAnswer(invocation -> localPrefValue.get() != null && localPrefValue.get());
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED);
    }

    private void onActivity(Activity activity) {
        mActivity = (TestActivity) activity;
    }

    @Test
    @SmallTest
    public void testBottomButtonHighlight() {
        launchFragmentWithArgs(
                AddressBarSettingsFragment.createArguments(HighlightedOption.BOTTOM_TOOLBAR));

        ColorDrawable initialBackground = (ColorDrawable) mBottomButton.getBackground();
        assertEquals(
                SemanticColorUtils.getSettingsBackgroundColor(mActivity),
                initialBackground.getColor());

        // Run delayed animation that reverts the color.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        ColorDrawable finalBackground = (ColorDrawable) mBottomButton.getBackground();
        assertEquals(SemanticColorUtils.getDefaultBgColor(mActivity), finalBackground.getColor());
    }

    private void launchFragmentWithArgs(Bundle args) {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        mSettings =
                (AddressBarSettingsFragment)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        AddressBarSettingsFragment.class.getClassLoader(),
                                        AddressBarSettingsFragment.class.getName());
        mSettings.setArguments(args);

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
        mSharedPreferencesManager.writeInt(
                ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, ToolbarPositionAndSource.TOP_SETTINGS);

        launchFragmentWithArgs(null);
        assertEquals(
                mActivity.getString(R.string.address_bar_settings_description),
                mAddressBarTitle.getSummary());
        assertTrue(mTopButton.isChecked());
        assertFalse(mBottomButton.isChecked());
        assertTrue(mToolbarPositionImage.isSelected());
        assertEquals(
                mActivity.getString(R.string.address_bar_settings_currently_on_top),
                mToolbarPositionImage.getContentDescription());
        clearInvocations(mLocalPrefService);

        mBottomButton.performClick();

        assertFalse(mTopButton.isChecked());
        assertTrue(mBottomButton.isChecked());
        assertEquals(
                ToolbarPositionAndSource.BOTTOM_SETTINGS,
                mSharedPreferencesManager.readInt(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED));
        assertFalse(mToolbarPositionImage.isSelected());
        assertEquals(
                mActivity.getString(R.string.address_bar_settings_currently_on_bottom),
                mToolbarPositionImage.getContentDescription());
        verify(mLocalPrefService, times(1)).setBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, true);
    }

    @Test
    @SmallTest
    public void testBottomAndThenSelectTop() {
        mSharedPreferencesManager.writeInt(
                ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                ToolbarPositionAndSource.BOTTOM_SETTINGS);

        launchFragmentWithArgs(null);
        assertFalse(mTopButton.isChecked());
        assertTrue(mBottomButton.isChecked());
        assertFalse(mToolbarPositionImage.isSelected());
        clearInvocations(mLocalPrefService);

        mTopButton.performClick();

        assertTrue(mTopButton.isChecked());
        assertFalse(mBottomButton.isChecked());
        assertEquals(
                ToolbarPositionAndSource.TOP_SETTINGS,
                mSharedPreferencesManager.readInt(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED));
        assertTrue(mToolbarPositionImage.isSelected());
        verify(mLocalPrefService, times(1)).setBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        verify(mLocalPrefService, never()).setBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, true);
    }

    @Test
    @SmallTest
    public void testBottomAndThenSelectTop_localPrefNotInitialized() {
        LocalStatePrefs.setNativePrefsLoadedForTesting(false);
        mSharedPreferencesManager.writeInt(
                ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                ToolbarPositionAndSource.BOTTOM_SETTINGS);

        launchFragmentWithArgs(null);
        assertFalse(mTopButton.isChecked());
        assertTrue(mBottomButton.isChecked());
        assertFalse(mToolbarPositionImage.isSelected());
        clearInvocations(mLocalPrefService);

        mTopButton.performClick();

        assertTrue(mTopButton.isChecked());
        assertFalse(mBottomButton.isChecked());
        assertEquals(
                ToolbarPositionAndSource.TOP_SETTINGS,
                mSharedPreferencesManager.readInt(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED));
        assertTrue(mToolbarPositionImage.isSelected());
        verify(mLocalStatePrefsNatives, never()).getPrefService();
    }

    @Test
    @SmallTest
    @Config(sdk = android.os.Build.VERSION_CODES.R)
    public void testFoldable() {
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE, true);
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);

        launchFragmentWithArgs(null);
        assertEquals(
                mActivity.getString(R.string.address_bar_settings_description_foldable),
                mAddressBarTitle.getSummary());
    }
}
