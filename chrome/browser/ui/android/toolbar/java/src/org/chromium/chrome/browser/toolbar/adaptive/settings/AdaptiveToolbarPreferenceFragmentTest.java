// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS;

import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link AdaptiveToolbarPreferenceFragment}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
@DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
public class AdaptiveToolbarPreferenceFragmentTest {
    @Rule
    public SettingsActivityTestRule<AdaptiveToolbarPreferenceFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AdaptiveToolbarPreferenceFragment.class);

    private AdaptiveToolbarPreferenceFragment mSettings;
    private ChromeSwitchPreference mSwitchPreference;
    private RadioButtonGroupAdaptiveToolbarPreference mRadioPreference;

    @Before
    public void setUpTest() throws Exception {
        SharedPreferencesManager.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED);
        SharedPreferencesManager.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(false, AdaptiveToolbarButtonVariant.NEW_TAB));
        mSettingsActivityTestRule.startSettingsActivity();
        mSettings = mSettingsActivityTestRule.getFragment();
        mSwitchPreference = (ChromeSwitchPreference) mSettings.findPreference(
                AdaptiveToolbarPreferenceFragment.PREF_TOOLBAR_SHORTCUT_SWITCH);
        mRadioPreference = (RadioButtonGroupAdaptiveToolbarPreference) mSettings.findPreference(
                AdaptiveToolbarPreferenceFragment.PREF_ADAPTIVE_RADIO_GROUP);
    }

    @After
    public void tearDownTest() throws Exception {
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SharedPreferencesManager.getInstance().removeKey(
                    ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED);
            SharedPreferencesManager.getInstance().removeKey(
                    ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS);
        });
    }

    @Test
    @SmallTest
    @Feature({"AdaptiveToolbar"})
    public void testSelectShortcuts() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(SharedPreferencesManager.getInstance().contains(
                    ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED));
            Assert.assertTrue(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());

            mSwitchPreference.performClick();
            Assert.assertFalse(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());
            Assert.assertTrue(SharedPreferencesManager.getInstance().contains(
                    ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED));
            Assert.assertFalse(SharedPreferencesManager.getInstance().readBoolean(
                    ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED, false));

            mSwitchPreference.performClick();
            Assert.assertTrue(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());
            Assert.assertTrue(SharedPreferencesManager.getInstance().readBoolean(
                    ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED, false));

            int expectedDefaultShortcut = AdaptiveToolbarButtonVariant.AUTO;
            Assert.assertEquals("Incorrect default setting.", expectedDefaultShortcut,
                    AdaptiveToolbarPrefs.getCustomizationSetting());
            assertButtonCheckedCorrectly("Based on your usage", expectedDefaultShortcut);

            // Select Based on your usage
            Assert.assertEquals(R.id.adaptive_option_based_on_usage,
                    getButton(AdaptiveToolbarButtonVariant.AUTO).getId());
            selectButton(AdaptiveToolbarButtonVariant.AUTO);
            assertButtonCheckedCorrectly("Based on your usage", AdaptiveToolbarButtonVariant.AUTO);
            Assert.assertEquals(AdaptiveToolbarButtonVariant.AUTO, mRadioPreference.getSelection());
            Assert.assertEquals(AdaptiveToolbarButtonVariant.AUTO,
                    SharedPreferencesManager.getInstance().readInt(
                            ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));

            // Select New tab
            Assert.assertEquals(R.id.adaptive_option_new_tab,
                    getButton(AdaptiveToolbarButtonVariant.NEW_TAB).getId());
            selectButton(AdaptiveToolbarButtonVariant.NEW_TAB);
            assertButtonCheckedCorrectly("New tab", AdaptiveToolbarButtonVariant.NEW_TAB);
            Assert.assertEquals(
                    AdaptiveToolbarButtonVariant.NEW_TAB, mRadioPreference.getSelection());
            Assert.assertEquals(AdaptiveToolbarButtonVariant.NEW_TAB,
                    SharedPreferencesManager.getInstance().readInt(
                            ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));

            // Select Share
            Assert.assertEquals(R.id.adaptive_option_share,
                    getButton(AdaptiveToolbarButtonVariant.SHARE).getId());
            selectButton(AdaptiveToolbarButtonVariant.SHARE);
            assertButtonCheckedCorrectly("Share", AdaptiveToolbarButtonVariant.SHARE);
            Assert.assertEquals(
                    AdaptiveToolbarButtonVariant.SHARE, mRadioPreference.getSelection());
            Assert.assertEquals(AdaptiveToolbarButtonVariant.SHARE,
                    SharedPreferencesManager.getInstance().readInt(
                            ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));

            // Select Voice search
            Assert.assertEquals(R.id.adaptive_option_voice_search,
                    getButton(AdaptiveToolbarButtonVariant.VOICE).getId());
            selectButton(AdaptiveToolbarButtonVariant.VOICE);
            assertButtonCheckedCorrectly("Voice search", AdaptiveToolbarButtonVariant.VOICE);
            Assert.assertEquals(
                    AdaptiveToolbarButtonVariant.VOICE, mRadioPreference.getSelection());
            Assert.assertEquals(AdaptiveToolbarButtonVariant.VOICE,
                    SharedPreferencesManager.getInstance().readInt(
                            ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));
        });
    }

    private RadioButtonWithDescription getButton(@AdaptiveToolbarButtonVariant int type) {
        return (RadioButtonWithDescription) mRadioPreference.getButton(type);
    }

    private void selectButton(@AdaptiveToolbarButtonVariant int type) {
        getButton(type).onClick(null);
    }

    private boolean isRestUnchecked(@AdaptiveToolbarButtonVariant int selectedType) {
        for (int i = 0; i < AdaptiveToolbarButtonVariant.NUM_ENTRIES; i++) {
            RadioButtonWithDescription button = getButton(i);
            if (i != selectedType && button != null && button.isChecked()) {
                return false;
            }
        }
        return true;
    }

    private void assertButtonCheckedCorrectly(
            String buttonTitle, @AdaptiveToolbarButtonVariant int type) {
        Assert.assertTrue(buttonTitle + " button should be checked.", getButton(type).isChecked());
        Assert.assertTrue(
                "Buttons except " + buttonTitle + " should be unchecked.", isRestUnchecked(type));
    }
}