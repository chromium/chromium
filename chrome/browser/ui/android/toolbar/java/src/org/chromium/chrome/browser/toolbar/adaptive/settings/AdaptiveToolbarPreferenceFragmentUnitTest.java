// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import static org.junit.Assert.assertEquals;

import android.os.Looper;
import android.util.Pair;
import android.view.View;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.viewpager2.adapter.FragmentStateAdapter;
import androidx.viewpager2.widget.ViewPager2;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;

/**
 * Unit tests for {@link AdaptiveToolbarPreferenceFragment} and {@link
 * RadioButtonGroupAdaptiveToolbarPreference}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
public class AdaptiveToolbarPreferenceFragmentUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private FragmentActivity mTestActivity;
    private AdaptiveToolbarPreferenceFragment mFragment;

    private static class TestFragmentAdapter extends FragmentStateAdapter {
        private AdaptiveToolbarPreferenceFragment mLastFragment;

        public TestFragmentAdapter(FragmentActivity fragmentActivity) {
            super(fragmentActivity);
        }

        @Override
        public Fragment createFragment(int i) {
            mLastFragment = new AdaptiveToolbarPreferenceFragment();
            return mLastFragment;
        }

        public AdaptiveToolbarPreferenceFragment getLastFragment() {
            return mLastFragment;
        }

        @Override
        public int getItemCount() {
            return 1;
        }
    }

    @Before
    public void setUp() {
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, AdaptiveToolbarButtonVariant.NEW_TAB));

        mTestActivity = Robolectric.setupActivity(FragmentActivity.class);
        ViewPager2 view = new ViewPager2(mTestActivity);
        mTestActivity.setContentView(view);

        TestFragmentAdapter adapter = new TestFragmentAdapter(mTestActivity);
        view.setAdapter(adapter);
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        mFragment = adapter.getLastFragment();
    }

    @After
    public void tearDown() {
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(null);
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @DisableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY})
    public void testCanUseVoiceSearch_enabled() {
        mFragment.setCanUseVoiceSearchForTesting(true);

        RadioButtonGroupAdaptiveToolbarPreference radioButtonGroup = mFragment.findPreference(
                AdaptiveToolbarPreferenceFragment.PREF_ADAPTIVE_RADIO_GROUP);
        RadioButtonWithDescription voiceButton =
                radioButtonGroup.getButton(AdaptiveToolbarButtonVariant.VOICE);

        assertEquals(View.VISIBLE, voiceButton.getVisibility());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @DisableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY})
    public void testCanUseVoiceSearch_disabled() {
        mFragment.setCanUseVoiceSearchForTesting(false);

        RadioButtonGroupAdaptiveToolbarPreference radioButtonGroup = mFragment.findPreference(
                AdaptiveToolbarPreferenceFragment.PREF_ADAPTIVE_RADIO_GROUP);
        RadioButtonWithDescription voiceButton =
                radioButtonGroup.getButton(AdaptiveToolbarButtonVariant.VOICE);

        assertEquals(View.GONE, voiceButton.getVisibility());
    }
}
