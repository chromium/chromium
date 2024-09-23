// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.settings_promo_card;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.preference.PreferenceViewHolder;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;

/** Test for {@link SettingsPromoCardPreference}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
public class SettingsPromoCardPreferenceTest {
    @Mock private Tracker mTestTracker;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private Activity mActivity;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    public void testDefaultBrowserPromoNotShown() {
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(any())).thenReturn(true);
        when(mTestTracker.shouldTriggerHelpUI(any())).thenReturn(false);

        SettingsPromoCardPreference preference =
                new SettingsPromoCardPreference(mActivity, null, mTestTracker);
        View itemView = new FrameLayout(mActivity);
        PreferenceViewHolder viewHolder = PreferenceViewHolder.createInstanceForTests(itemView);
        preference.onBindViewHolder(viewHolder);

        Assert.assertFalse(preference.isVisible());
    }

    @Test
    public void testDefaultBrowserPromoShown() {
        when(mTestTracker.shouldTriggerHelpUI(any())).thenReturn(true);
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(any())).thenReturn(true);

        SettingsPromoCardPreference preference =
                new SettingsPromoCardPreference(mActivity, null, mTestTracker);
        Assert.assertTrue(preference.isVisible());

        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(any())).thenReturn(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    preference.updatePreferences();
                });
        Assert.assertFalse(preference.isVisible());
    }
}
