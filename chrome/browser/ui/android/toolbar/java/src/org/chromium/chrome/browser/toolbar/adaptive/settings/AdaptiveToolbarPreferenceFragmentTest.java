// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS;

import android.os.Bundle;
import android.util.Pair;
import android.view.View;

import androidx.fragment.app.testing.FragmentScenario;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;

/**
 * Tests for {@link AdaptiveToolbarPreferenceFragment}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
@DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR,
        ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_TRANSLATE,
        ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_ADD_TO_BOOKMARKS})
public class AdaptiveToolbarPreferenceFragmentTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private ChromeSwitchPreference mSwitchPreference;
    private RadioButtonGroupAdaptiveToolbarPreference mRadioPreference;

    @Before
    public void setUpTest() throws Exception {
        SharedPreferencesManager.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED);
        SharedPreferencesManager.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(false, AdaptiveToolbarButtonVariant.NEW_TAB));

        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(true);
    }

    @After
    public void tearDownTest() throws Exception {
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(null);
        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(null);
        SharedPreferencesManager.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED);
        SharedPreferencesManager.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS);
    }

    @Test
    @SmallTest
    public void testSelectShortcuts() {
        FragmentScenario<AdaptiveToolbarPreferenceFragment> scenario =
                FragmentScenario.launchInContainer(AdaptiveToolbarPreferenceFragment.class,
                        Bundle.EMPTY, R.style.Theme_Chromium_Settings);
        scenario.onFragment(fragment -> {
            mSwitchPreference = (ChromeSwitchPreference) fragment.findPreference(
                    AdaptiveToolbarPreferenceFragment.PREF_TOOLBAR_SHORTCUT_SWITCH);
            mRadioPreference = (RadioButtonGroupAdaptiveToolbarPreference) fragment.findPreference(
                    AdaptiveToolbarPreferenceFragment.PREF_ADAPTIVE_RADIO_GROUP);

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

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_TRANSLATE)
    public void testTranslateOption_Enabled() {
        FragmentScenario<AdaptiveToolbarPreferenceFragment> scenario =
                FragmentScenario.launchInContainer(AdaptiveToolbarPreferenceFragment.class,
                        Bundle.EMPTY, R.style.Theme_Chromium_Settings);
        scenario.onFragment(fragment -> {
            mRadioPreference = (RadioButtonGroupAdaptiveToolbarPreference) fragment.findPreference(
                    AdaptiveToolbarPreferenceFragment.PREF_ADAPTIVE_RADIO_GROUP);

            // Select Translate.
            Assert.assertEquals(R.id.adaptive_option_translate,
                    getButton(AdaptiveToolbarButtonVariant.TRANSLATE).getId());
            selectButton(AdaptiveToolbarButtonVariant.TRANSLATE);
            assertButtonCheckedCorrectly("Translate", AdaptiveToolbarButtonVariant.TRANSLATE);
            Assert.assertEquals(
                    AdaptiveToolbarButtonVariant.TRANSLATE, mRadioPreference.getSelection());
            Assert.assertEquals(AdaptiveToolbarButtonVariant.TRANSLATE,
                    SharedPreferencesManager.getInstance().readInt(
                            ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));
        });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_TRANSLATE)
    public void testTranslateOption_Disabled() {
        // Set initial preference to translate.
        SharedPreferencesManager.getInstance().writeInt(
                ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS, AdaptiveToolbarButtonVariant.TRANSLATE);
        FragmentScenario<AdaptiveToolbarPreferenceFragment> scenario =
                FragmentScenario.launchInContainer(AdaptiveToolbarPreferenceFragment.class,
                        Bundle.EMPTY, R.style.Theme_Chromium_Settings);
        scenario.onFragment(fragment -> {
            mRadioPreference = (RadioButtonGroupAdaptiveToolbarPreference) fragment.findPreference(
                    AdaptiveToolbarPreferenceFragment.PREF_ADAPTIVE_RADIO_GROUP);

            // Translate option should be hidden, and we should have reverted back to "Auto".
            Assert.assertEquals(R.id.adaptive_option_translate,
                    getButton(AdaptiveToolbarButtonVariant.TRANSLATE).getId());
            Assert.assertEquals(
                    View.GONE, getButton(AdaptiveToolbarButtonVariant.TRANSLATE).getVisibility());
            assertButtonCheckedCorrectly("Based on your usage", AdaptiveToolbarButtonVariant.AUTO);
            Assert.assertEquals(AdaptiveToolbarButtonVariant.AUTO, mRadioPreference.getSelection());
            Assert.assertEquals(AdaptiveToolbarButtonVariant.AUTO,
                    SharedPreferencesManager.getInstance().readInt(
                            ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_ADD_TO_BOOKMARKS)
    public void testAddToBookmarksOption_Enabled() {
        FragmentScenario<AdaptiveToolbarPreferenceFragment> scenario =
                FragmentScenario.launchInContainer(AdaptiveToolbarPreferenceFragment.class,
                        Bundle.EMPTY, R.style.Theme_Chromium_Settings);
        scenario.onFragment(fragment -> {
            mRadioPreference = (RadioButtonGroupAdaptiveToolbarPreference) fragment.findPreference(
                    AdaptiveToolbarPreferenceFragment.PREF_ADAPTIVE_RADIO_GROUP);

            // Select Add to bookmarks.
            Assert.assertEquals(R.id.adaptive_option_add_to_bookmarks,
                    getButton(AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS).getId());
            selectButton(AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS);
            assertButtonCheckedCorrectly(
                    "Add to bookmarks", AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS);
            Assert.assertEquals(
                    AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS, mRadioPreference.getSelection());
            Assert.assertEquals(AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                    SharedPreferencesManager.getInstance().readInt(
                            ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));
        });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_ADD_TO_BOOKMARKS)
    public void testAddToBookmarksOption_Disabled() {
        // Set initial preference to add to bookmarks.
        SharedPreferencesManager.getInstance().writeInt(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS,
                AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS);
        FragmentScenario<AdaptiveToolbarPreferenceFragment> scenario =
                FragmentScenario.launchInContainer(AdaptiveToolbarPreferenceFragment.class,
                        Bundle.EMPTY, R.style.Theme_Chromium_Settings);
        scenario.onFragment(fragment -> {
            mRadioPreference = (RadioButtonGroupAdaptiveToolbarPreference) fragment.findPreference(
                    AdaptiveToolbarPreferenceFragment.PREF_ADAPTIVE_RADIO_GROUP);

            // Add to bookmarks option should be hidden, and we should have reverted back to "Auto".
            Assert.assertEquals(R.id.adaptive_option_add_to_bookmarks,
                    getButton(AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS).getId());
            Assert.assertEquals(View.GONE,
                    getButton(AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS).getVisibility());
            assertButtonCheckedCorrectly("Based on your usage", AdaptiveToolbarButtonVariant.AUTO);
            Assert.assertEquals(AdaptiveToolbarButtonVariant.AUTO, mRadioPreference.getSelection());
            Assert.assertEquals(AdaptiveToolbarButtonVariant.AUTO,
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
        for (int i = 0; i <= AdaptiveToolbarButtonVariant.MAX_VALUE; i++) {
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
