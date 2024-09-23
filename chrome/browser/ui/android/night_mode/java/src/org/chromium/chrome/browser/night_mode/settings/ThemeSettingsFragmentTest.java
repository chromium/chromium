// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode.settings;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING;

import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.R;
import org.chromium.chrome.browser.night_mode.ThemeType;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.feature_engagement.Tracker;

/** Tests for ThemeSettingsFragment. */
@RunWith(BaseJUnit4ClassRunner.class)
@DisableFeatures(DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
public class ThemeSettingsFragmentTest {

    @Rule
    public BlankUiTestActivitySettingsTestRule mSettingsTestRule =
            new BlankUiTestActivitySettingsTestRule();

    @Rule public JniMocker mMocker = new JniMocker();

    @Mock public WebsitePreferenceBridge.Natives mMockWebsitePreferenceBridgeJni;
    @Mock public Profile mProfile;
    @Mock public Tracker mTracker;

    private ThemeSettingsFragment mFragment;
    private RadioButtonGroupThemePreference mPreference;

    // Boolean used for web content auto dark mode.
    private boolean mForceDarkModeEnabled;

    @Before
    public void setUp() {
        // For some reason MockitoRule does not work with JniMocker (seems like an order issue), and
        // RuleChain cannot be applied to MockitoRule since it is not a TestRule.
        MockitoAnnotations.initMocks(this);
        ChromeSharedPreferences.getInstance().removeKey(UI_THEME_SETTING);

        mMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mMockWebsitePreferenceBridgeJni);

        TrackerFactory.setTrackerForTests(mTracker);

        // Default value for feature DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING.
        mForceDarkModeEnabled = true;
        Mockito.doAnswer(invocation -> mForceDarkModeEnabled)
                .when(mMockWebsitePreferenceBridgeJni)
                .isContentSettingEnabled(any(), eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT));
        Mockito.doAnswer(
                        invocation -> {
                            mForceDarkModeEnabled = (boolean) invocation.getArguments()[2];
                            return null;
                        })
                .when(mMockWebsitePreferenceBridgeJni)
                .setContentSettingEnabled(
                        any(), eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT), anyBoolean());
    }

    @Test
    @SmallTest
    @Feature({"Themes"})
    public void testSelectThemes() {
        launchThemeSettings(ThemeSettingsEntry.SETTINGS);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int expectedDefaultTheme = ThemeType.LIGHT;
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                        Assert.assertFalse(
                                "Q should not default to light.",
                                NightModeUtils.isNightModeDefaultToLight());
                        expectedDefaultTheme = ThemeType.SYSTEM_DEFAULT;
                    }

                    Assert.assertEquals(
                            "Incorrect default theme setting.",
                            expectedDefaultTheme,
                            NightModeUtils.getThemeSetting());
                    assertButtonCheckedCorrectly("Light", expectedDefaultTheme);

                    // Select System default
                    Assert.assertEquals(R.id.system_default, getButton(0).getId());
                    selectButton(0);
                    assertButtonCheckedCorrectly("System default", 0);
                    Assert.assertEquals(ThemeType.SYSTEM_DEFAULT, mPreference.getSetting());
                    Assert.assertEquals(
                            mPreference.getSetting(),
                            ChromeSharedPreferences.getInstance().readInt(UI_THEME_SETTING));

                    // Select Light
                    Assert.assertEquals(R.id.light, getButton(1).getId());
                    selectButton(1);
                    assertButtonCheckedCorrectly("Light", 1);
                    Assert.assertEquals(ThemeType.LIGHT, mPreference.getSetting());
                    Assert.assertEquals(
                            mPreference.getSetting(),
                            ChromeSharedPreferences.getInstance().readInt(UI_THEME_SETTING));

                    // Select Dark
                    Assert.assertEquals(R.id.dark, getButton(2).getId());
                    selectButton(2);
                    assertButtonCheckedCorrectly("Dark", 2);
                    Assert.assertEquals(ThemeType.DARK, mPreference.getSetting());
                    Assert.assertEquals(
                            mPreference.getSetting(),
                            ChromeSharedPreferences.getInstance().readInt(UI_THEME_SETTING));
                });
    }

    @Test
    @SmallTest
    @Feature({"Themes"})
    @EnableFeatures(DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    public void testDarkenWebsiteButton() {
        launchThemeSettings(ThemeSettingsEntry.SETTINGS);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int expectedDefaultTheme = ThemeType.LIGHT;
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                        Assert.assertFalse(
                                "Q should not default to light.",
                                NightModeUtils.isNightModeDefaultToLight());
                        expectedDefaultTheme = ThemeType.SYSTEM_DEFAULT;
                    }

                    LinearLayout checkboxContainer = mPreference.getCheckboxContainerForTesting();
                    RadioButtonWithDescriptionLayout group = mPreference.getGroupForTesting();

                    Assert.assertEquals(
                            "Incorrect default theme setting.",
                            expectedDefaultTheme,
                            NightModeUtils.getThemeSetting());
                    assertButtonCheckedCorrectly("Light", expectedDefaultTheme);

                    // Select System default
                    selectButton(0);
                    Assert.assertTrue(
                            "Darken website button should be visible when system default is"
                                    + " checked",
                            checkboxContainer.getVisibility() == View.VISIBLE);
                    Assert.assertEquals(
                            "Darken website button should be below the system default option when"
                                    + " system default is checked",
                            1,
                            group.indexOfChild(checkboxContainer));

                    // Select Light
                    selectButton(1);
                    Assert.assertTrue(
                            "Darken website button should be invisible when light theme is checked",
                            checkboxContainer.getVisibility() != View.VISIBLE);

                    // Select Dark
                    selectButton(2);
                    Assert.assertTrue(
                            "Darken website button should be visible when dark theme is checked",
                            checkboxContainer.getVisibility() == View.VISIBLE);
                    Assert.assertEquals(
                            "Darken website button should be below the dark theme option"
                                    + " when dark theme is checked",
                            3,
                            group.indexOfChild(checkboxContainer));

                    // Checks for Darken website button.
                    Assert.assertTrue(
                            "Darken website check box should be checked by default.",
                            mPreference.isDarkenWebsitesEnabled());

                    // Toggle the check box.
                    checkboxContainer.performClick();
                    Assert.assertFalse(
                            "Darken website check box should be unchecked.",
                            mPreference.isDarkenWebsitesEnabled());
                    Mockito.verify(mMockWebsitePreferenceBridgeJni, Mockito.times(1))
                            .setContentSettingEnabled(
                                    any(),
                                    eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT),
                                    eq(false));

                    checkboxContainer.performClick();
                    Assert.assertTrue(
                            "Darken website check box should be checked again.",
                            mPreference.isDarkenWebsitesEnabled());
                    Mockito.verify(mMockWebsitePreferenceBridgeJni, Mockito.times(1))
                            .setContentSettingEnabled(
                                    any(), eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT), eq(true));

                    // Check system default, darken website button should stay checked.
                    selectButton(1);
                    Assert.assertTrue(
                            "Darken website button should stay its state when changing theme"
                                    + " preference.",
                            mPreference.isDarkenWebsitesEnabled());
                    Mockito.verify(mMockWebsitePreferenceBridgeJni, Mockito.times(1))
                            .setContentSettingEnabled(
                                    any(), eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT), eq(true));
                });
    }

    @Test
    @SmallTest
    public void testStartThemeSettings_FromAutoDarkMessages() {
        launchThemeSettings(ThemeSettingsEntry.AUTO_DARK_MODE_MESSAGE);
    }

    private void launchThemeSettings(@ThemeSettingsEntry Integer settingsEntry) {
        Bundle args = new Bundle();
        args.putInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY, settingsEntry);
        mSettingsTestRule.launchPreference(
                ThemeSettingsFragment.class,
                args,
                (fragment) -> ((ThemeSettingsFragment) fragment).setProfile(mProfile));

        mFragment = (ThemeSettingsFragment) mSettingsTestRule.getPreferenceFragment();
        mPreference =
                (RadioButtonGroupThemePreference)
                        mFragment.findPreference(ThemeSettingsFragment.PREF_UI_THEME_PREF);
        assertThemeSettingsEntryRecorded(settingsEntry);
    }

    private RadioButtonWithDescription getButton(int index) {
        return (RadioButtonWithDescription) mPreference.getButtonsForTesting().get(index);
    }

    private void selectButton(int index) {
        getButton(index).onClick(null);
    }

    private boolean isRestUnchecked(int selectedIndex) {
        for (int i = 0; i < ThemeType.NUM_ENTRIES; i++) {
            if (i != selectedIndex && getButton(i).isChecked()) {
                return false;
            }
        }
        return true;
    }

    private void assertButtonCheckedCorrectly(String buttonTitle, int index) {
        Assert.assertTrue(buttonTitle + " button should be checked.", getButton(index).isChecked());
        Assert.assertTrue(
                "Buttons except " + buttonTitle + " should be unchecked.", isRestUnchecked(index));
    }

    private void assertThemeSettingsEntryRecorded(int sample) {
        Assert.assertEquals(
                "<Android.DarkTheme.ThemeSettingsEntry> should be recorded once.",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.DarkTheme.ThemeSettingsEntry"));
        Assert.assertEquals(
                "<Android.DarkTheme.ThemeSettingsEntry> should be recorded once for sample <"
                        + sample
                        + ">.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.DarkTheme.ThemeSettingsEntry", sample));
    }
}
