// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

import android.content.Context;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.SettingsStylingController.BackgroundStyleDetails;

import java.util.ArrayList;

/** Tests for {@link SettingsStylingController}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SettingsStylingControllerTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private int mOuterRadius;
    private int mInnerRadius;

    private Context mContext;
    private SettingsStylingController mController;
    private PreferenceScreen mPreferenceScreen;

    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mContext = mSettingsRule.getActivity();
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
        mController = new SettingsStylingController(mContext, mPreferenceScreen);
        mOuterRadius =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_outer);
        mInnerRadius =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_inner);
    }

    private Preference createPreference(boolean visible) {
        Preference preference = new Preference(mContext);
        preference.setVisible(visible);
        return preference;
    }

    private PreferenceCategory createPreferenceCategory(boolean visible) {
        PreferenceCategory preference = new PreferenceCategory(mContext);
        preference.setVisible(visible);
        return preference;
    }

    private TextMessagePreference createTextMessagePreference(boolean visible) {
        TextMessagePreference textMessagePreference = new TextMessagePreference(mContext, null);
        textMessagePreference.setVisible(visible);
        return textMessagePreference;
    }

    @Test
    @SmallTest
    public void testBackgroundStyle_WithSingleItem() {
        mPreferenceScreen.addPreference(createPreference(true));

        ArrayList<BackgroundStyleDetails> styles = mController.generateBackgroundStyleDetails();
        assertEquals(1, styles.size());
        assertEquals(mOuterRadius, styles.get(0).topRadius, 0);
        assertEquals(mOuterRadius, styles.get(0).bottomRadius, 0);
    }

    @Test
    @SmallTest
    public void testBackgroundStyle_WithCategorySeparator() {
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createPreferenceCategory(true));
        mPreferenceScreen.addPreference(createPreference(true));

        ArrayList<BackgroundStyleDetails> styles = mController.generateBackgroundStyleDetails();
        assertEquals(3, styles.size());
        assertEquals(mOuterRadius, styles.get(0).topRadius, 0);
        assertEquals(mOuterRadius, styles.get(0).bottomRadius, 0);
        assertSame(BackgroundStyleDetails.EMPTY, styles.get(1));
        assertEquals(mOuterRadius, styles.get(2).topRadius, 0);
        assertEquals(mOuterRadius, styles.get(2).bottomRadius, 0);
    }

    @Test
    @SmallTest
    public void testBackgroundStyle_WithTextMessagePreference() {
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createTextMessagePreference(true));

        ArrayList<BackgroundStyleDetails> styles = mController.generateBackgroundStyleDetails();
        assertEquals(2, styles.size());
        assertEquals(mOuterRadius, styles.get(0).topRadius, 0);
        assertEquals(mOuterRadius, styles.get(0).bottomRadius, 0);
        assertSame(BackgroundStyleDetails.EMPTY, styles.get(1));
    }

    @Test
    @SmallTest
    public void testBackgroundStyle_WithComplexLayout() {
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createPreferenceCategory(true));
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createPreferenceCategory(true));
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createTextMessagePreference(true));

        ArrayList<BackgroundStyleDetails> styles = mController.generateBackgroundStyleDetails();

        // PrefA, PrefB
        assertEquals(mOuterRadius, styles.get(0).topRadius, 0);
        assertEquals(mInnerRadius, styles.get(0).bottomRadius, 0);
        assertEquals(mInnerRadius, styles.get(1).topRadius, 0);
        assertEquals(mOuterRadius, styles.get(1).bottomRadius, 0);

        // Category1
        assertSame(BackgroundStyleDetails.EMPTY, styles.get(2));

        // PrefC
        assertEquals(mOuterRadius, styles.get(3).topRadius, 0);
        assertEquals(mOuterRadius, styles.get(3).bottomRadius, 0);

        // Category2
        assertSame(BackgroundStyleDetails.EMPTY, styles.get(4));

        // PrefD, PrefE, PrefF
        assertEquals(mOuterRadius, styles.get(5).topRadius, 0);
        assertEquals(mInnerRadius, styles.get(5).bottomRadius, 0);
        assertEquals(mInnerRadius, styles.get(6).topRadius, 0);
        assertEquals(mInnerRadius, styles.get(6).bottomRadius, 0);
        assertEquals(mInnerRadius, styles.get(7).topRadius, 0);
        assertEquals(mOuterRadius, styles.get(7).bottomRadius, 0);

        // Text Preference
        assertSame(BackgroundStyleDetails.EMPTY, styles.get(4));
    }

    @Test
    @SmallTest
    public void testBackgroundStyle_WithInvisibleItems() {
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createPreference(false));
        mPreferenceScreen.addPreference(createPreference(true));

        ArrayList<BackgroundStyleDetails> styles = mController.generateBackgroundStyleDetails();
        assertEquals(2, styles.size());
        assertEquals(mOuterRadius, styles.get(0).topRadius, 0);
        assertEquals(mInnerRadius, styles.get(0).bottomRadius, 0);
        assertEquals(mInnerRadius, styles.get(1).topRadius, 0);
        assertEquals(mOuterRadius, styles.get(1).bottomRadius, 0);
    }
}
