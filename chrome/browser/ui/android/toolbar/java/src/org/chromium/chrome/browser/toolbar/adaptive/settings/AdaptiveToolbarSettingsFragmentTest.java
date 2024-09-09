// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS;

import android.os.Bundle;
import android.util.Pair;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.List;

/** Tests for {@link AdaptiveToolbarSettingsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
@DisableFeatures({
    ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY,
    ChromeFeatureList.READALOUD
})
public class AdaptiveToolbarSettingsFragmentTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock Profile mProfile;
    @Mock private UserPrefsJni mUserPrefsNatives;
    @Mock private PrefService mPrefService;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mSearchEngine;

    private ChromeSwitchPreference mSwitchPreference;
    private RadioButtonGroupAdaptiveToolbarPreference mRadioPreference;

    @Before
    public void setUpTest() throws Exception {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        doReturn(mPrefService).when(mUserPrefsNatives).get(any());

        ChromeSharedPreferences.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED);
        ChromeSharedPreferences.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(
                        false,
                        List.of(
                                AdaptiveToolbarButtonVariant.NEW_TAB,
                                AdaptiveToolbarButtonVariant.SHARE)));

        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(true);
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(true);

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
    }

    @After
    public void tearDownTest() throws Exception {
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(null);
        ChromeSharedPreferences.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED);
        ChromeSharedPreferences.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS);
    }

    @Test
    @SmallTest
    public void testSelectShortcuts() {
        FragmentScenario<AdaptiveToolbarSettingsFragment> scenario = buildFragmentScenario();
        scenario.onFragment(
                fragment -> {
                    mSwitchPreference =
                            (ChromeSwitchPreference)
                                    fragment.findPreference(
                                            AdaptiveToolbarSettingsFragment
                                                    .PREF_TOOLBAR_SHORTCUT_SWITCH);
                    mRadioPreference =
                            (RadioButtonGroupAdaptiveToolbarPreference)
                                    fragment.findPreference(
                                            AdaptiveToolbarSettingsFragment
                                                    .PREF_ADAPTIVE_RADIO_GROUP);

                    Assert.assertFalse(
                            ChromeSharedPreferences.getInstance()
                                    .contains(ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED));
                    Assert.assertTrue(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());

                    mSwitchPreference.performClick();
                    Assert.assertFalse(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());
                    Assert.assertTrue(
                            ChromeSharedPreferences.getInstance()
                                    .contains(ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED));
                    Assert.assertFalse(
                            ChromeSharedPreferences.getInstance()
                                    .readBoolean(ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED, false));

                    mSwitchPreference.performClick();
                    Assert.assertTrue(AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());
                    Assert.assertTrue(
                            ChromeSharedPreferences.getInstance()
                                    .readBoolean(ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED, false));

                    int expectedDefaultShortcut = AdaptiveToolbarButtonVariant.AUTO;
                    Assert.assertEquals(
                            "Incorrect default setting.",
                            expectedDefaultShortcut,
                            AdaptiveToolbarPrefs.getCustomizationSetting());
                    assertButtonCheckedCorrectly("Based on your usage", expectedDefaultShortcut);

                    // Select Based on your usage
                    Assert.assertEquals(
                            R.id.adaptive_option_based_on_usage,
                            getButton(AdaptiveToolbarButtonVariant.AUTO).getId());
                    selectButton(AdaptiveToolbarButtonVariant.AUTO);
                    assertButtonCheckedCorrectly(
                            "Based on your usage", AdaptiveToolbarButtonVariant.AUTO);
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.AUTO, mRadioPreference.getSelection());
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.AUTO,
                            ChromeSharedPreferences.getInstance()
                                    .readInt(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));

                    // Select New tab
                    Assert.assertEquals(
                            R.id.adaptive_option_new_tab,
                            getButton(AdaptiveToolbarButtonVariant.NEW_TAB).getId());
                    selectButton(AdaptiveToolbarButtonVariant.NEW_TAB);
                    assertButtonCheckedCorrectly("New tab", AdaptiveToolbarButtonVariant.NEW_TAB);
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.NEW_TAB, mRadioPreference.getSelection());
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.NEW_TAB,
                            ChromeSharedPreferences.getInstance()
                                    .readInt(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));

                    // Select Share
                    Assert.assertEquals(
                            R.id.adaptive_option_share,
                            getButton(AdaptiveToolbarButtonVariant.SHARE).getId());
                    selectButton(AdaptiveToolbarButtonVariant.SHARE);
                    assertButtonCheckedCorrectly("Share", AdaptiveToolbarButtonVariant.SHARE);
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.SHARE, mRadioPreference.getSelection());
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.SHARE,
                            ChromeSharedPreferences.getInstance()
                                    .readInt(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));

                    // Select Voice search
                    Assert.assertEquals(
                            R.id.adaptive_option_voice_search,
                            getButton(AdaptiveToolbarButtonVariant.VOICE).getId());
                    selectButton(AdaptiveToolbarButtonVariant.VOICE);
                    assertButtonCheckedCorrectly(
                            "Voice search", AdaptiveToolbarButtonVariant.VOICE);
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.VOICE, mRadioPreference.getSelection());
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.VOICE,
                            ChromeSharedPreferences.getInstance()
                                    .readInt(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.READALOUD)
    public void testReadAloudOption_Enabled() {
        FragmentScenario<AdaptiveToolbarSettingsFragment> scenario = buildFragmentScenario();
        scenario.onFragment(
                fragment -> {
                    mRadioPreference =
                            (RadioButtonGroupAdaptiveToolbarPreference)
                                    fragment.findPreference(
                                            AdaptiveToolbarSettingsFragment
                                                    .PREF_ADAPTIVE_RADIO_GROUP);

                    // Select Read Aloud.
                    Assert.assertEquals(
                            R.id.adaptive_option_read_aloud,
                            getButton(AdaptiveToolbarButtonVariant.READ_ALOUD).getId());
                    selectButton(AdaptiveToolbarButtonVariant.READ_ALOUD);
                    assertButtonCheckedCorrectly(
                            "Read Aloud", AdaptiveToolbarButtonVariant.READ_ALOUD);
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.READ_ALOUD,
                            mRadioPreference.getSelection());
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.READ_ALOUD,
                            ChromeSharedPreferences.getInstance()
                                    .readInt(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));
                });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.READALOUD)
    public void testReadAloudOption_Disabled() {
        // Set initial preference to Read Aloud.
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS,
                        AdaptiveToolbarButtonVariant.READ_ALOUD);
        FragmentScenario<AdaptiveToolbarSettingsFragment> scenario = buildFragmentScenario();
        scenario.onFragment(
                fragment -> {
                    mRadioPreference =
                            (RadioButtonGroupAdaptiveToolbarPreference)
                                    fragment.findPreference(
                                            AdaptiveToolbarSettingsFragment
                                                    .PREF_ADAPTIVE_RADIO_GROUP);

                    // Read Aloud option should be hidden, and we should have reverted back to
                    // "Auto".
                    Assert.assertEquals(
                            R.id.adaptive_option_read_aloud,
                            getButton(AdaptiveToolbarButtonVariant.READ_ALOUD).getId());

                    Assert.assertEquals(
                            View.GONE,
                            getButton(AdaptiveToolbarButtonVariant.READ_ALOUD).getVisibility());

                    assertButtonCheckedCorrectly(
                            "Based on your usage", AdaptiveToolbarButtonVariant.AUTO);
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.AUTO, mRadioPreference.getSelection());
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.AUTO,
                            ChromeSharedPreferences.getInstance()
                                    .readInt(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)
    public void testPageSummaryOption_Enabled() {
        FragmentScenario<AdaptiveToolbarSettingsFragment> scenario = buildFragmentScenario();
        scenario.onFragment(
                fragment -> {
                    mRadioPreference =
                            (RadioButtonGroupAdaptiveToolbarPreference)
                                    fragment.findPreference(
                                            AdaptiveToolbarSettingsFragment
                                                    .PREF_ADAPTIVE_RADIO_GROUP);

                    // Select Read Aloud.
                    Assert.assertEquals(
                            R.id.adaptive_option_page_summary,
                            getButton(AdaptiveToolbarButtonVariant.PAGE_SUMMARY).getId());
                    selectButton(AdaptiveToolbarButtonVariant.PAGE_SUMMARY);
                    assertButtonCheckedCorrectly(
                            "Page Summary", AdaptiveToolbarButtonVariant.PAGE_SUMMARY);
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.PAGE_SUMMARY,
                            mRadioPreference.getSelection());
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.PAGE_SUMMARY,
                            ChromeSharedPreferences.getInstance()
                                    .readInt(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));
                });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)
    public void testPageSummaryOption_Disabled() {
        // Set initial preference to page summary.
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS,
                        AdaptiveToolbarButtonVariant.PAGE_SUMMARY);
        FragmentScenario<AdaptiveToolbarSettingsFragment> scenario = buildFragmentScenario();
        scenario.onFragment(
                fragment -> {
                    mRadioPreference =
                            (RadioButtonGroupAdaptiveToolbarPreference)
                                    fragment.findPreference(
                                            AdaptiveToolbarSettingsFragment
                                                    .PREF_ADAPTIVE_RADIO_GROUP);

                    // Read Aloud option should be hidden, and we should have reverted back to
                    // "Auto".
                    Assert.assertEquals(
                            R.id.adaptive_option_page_summary,
                            getButton(AdaptiveToolbarButtonVariant.PAGE_SUMMARY).getId());

                    Assert.assertEquals(
                            View.GONE,
                            getButton(AdaptiveToolbarButtonVariant.PAGE_SUMMARY).getVisibility());

                    assertButtonCheckedCorrectly(
                            "Based on your usage", AdaptiveToolbarButtonVariant.AUTO);
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.AUTO, mRadioPreference.getSelection());
                    Assert.assertEquals(
                            AdaptiveToolbarButtonVariant.AUTO,
                            ChromeSharedPreferences.getInstance()
                                    .readInt(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS));
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

    private FragmentScenario<AdaptiveToolbarSettingsFragment> buildFragmentScenario() {
        return FragmentScenario.launchInContainer(
                AdaptiveToolbarSettingsFragment.class,
                Bundle.EMPTY,
                R.style.Theme_Chromium_Settings,
                new FragmentFactory() {
                    @Override
                    public Fragment instantiate(
                            @NonNull ClassLoader classLoader, @NonNull String className) {
                        Fragment fragment = super.instantiate(classLoader, className);
                        if (fragment instanceof ProfileDependentSetting) {
                            ((ProfileDependentSetting) fragment).setProfile(mProfile);
                        }
                        return fragment;
                    }
                });
    }
}
