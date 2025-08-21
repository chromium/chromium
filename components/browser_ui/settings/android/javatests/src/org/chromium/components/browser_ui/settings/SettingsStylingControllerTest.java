// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

import static org.chromium.components.browser_ui.settings.CustomStyledPreference.DEFAULT;

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
import org.chromium.components.browser_ui.settings.CustomStyledPreference.BackgroundStyle;

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

    private TextMessagePreference createCustomTextMessagePreference(
            @BackgroundStyle int backgroundStyle, int topMargin, int bottomMargin) {
        return new TextMessagePreference(mContext, null) {
            @Override
            public int getCustomBackgroundStyle() {
                return backgroundStyle;
            }

            @Override
            public int getCustomTopMargin() {
                return topMargin;
            }

            @Override
            public int getCustomBottomMargin() {
                return bottomMargin;
            }
        };
    }

    @Test
    @SmallTest
    public void testBackgroundStyle_WithSingleItem() {
        mPreferenceScreen.addPreference(createPreference(true));

        ArrayList<PreferenceStyle> styles = mController.generatePreferenceStyles();
        assertEquals(1, styles.size());
        assertEquals(mOuterRadius, styles.get(0).getTopRadius(), 0);
        assertEquals(mOuterRadius, styles.get(0).getBottomRadius(), 0);
    }

    @Test
    @SmallTest
    public void testBackgroundStyle_WithCategorySeparator() {
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createPreferenceCategory(true));
        mPreferenceScreen.addPreference(createPreference(true));

        ArrayList<PreferenceStyle> styles = mController.generatePreferenceStyles();
        assertEquals(3, styles.size());
        assertEquals(mOuterRadius, styles.get(0).getTopRadius(), 0);
        assertEquals(mOuterRadius, styles.get(0).getBottomRadius(), 0);
        assertSame(PreferenceStyle.EMPTY, styles.get(1));
        assertEquals(mOuterRadius, styles.get(2).getTopRadius(), 0);
        assertEquals(mOuterRadius, styles.get(2).getBottomRadius(), 0);
    }

    @Test
    @SmallTest
    public void testBackgroundStyle_WithTextMessagePreference() {
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createTextMessagePreference(true));

        ArrayList<PreferenceStyle> styles = mController.generatePreferenceStyles();
        assertEquals(2, styles.size());
        assertEquals(mOuterRadius, styles.get(0).getTopRadius(), 0);
        assertEquals(mOuterRadius, styles.get(0).getBottomRadius(), 0);
        assertSame(PreferenceStyle.EMPTY, styles.get(1));
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

        ArrayList<PreferenceStyle> styles = mController.generatePreferenceStyles();

        // PrefA, PrefB
        assertEquals(mOuterRadius, styles.get(0).getTopRadius(), 0);
        assertEquals(mInnerRadius, styles.get(0).getBottomRadius(), 0);
        assertEquals(mInnerRadius, styles.get(1).getTopRadius(), 0);
        assertEquals(mOuterRadius, styles.get(1).getBottomRadius(), 0);

        // Category1
        assertSame(PreferenceStyle.EMPTY, styles.get(2));

        // PrefC
        assertEquals(mOuterRadius, styles.get(3).getTopRadius(), 0);
        assertEquals(mOuterRadius, styles.get(3).getBottomRadius(), 0);

        // Category2
        assertSame(PreferenceStyle.EMPTY, styles.get(4));

        // PrefD, PrefE, PrefF
        assertEquals(mOuterRadius, styles.get(5).getTopRadius(), 0);
        assertEquals(mInnerRadius, styles.get(5).getBottomRadius(), 0);
        assertEquals(mInnerRadius, styles.get(6).getTopRadius(), 0);
        assertEquals(mInnerRadius, styles.get(6).getBottomRadius(), 0);
        assertEquals(mInnerRadius, styles.get(7).getTopRadius(), 0);
        assertEquals(mOuterRadius, styles.get(7).getBottomRadius(), 0);

        // Text Preference
        assertSame(PreferenceStyle.EMPTY, styles.get(8));
    }

    @Test
    @SmallTest
    public void testBackgroundStyle_WithInvisibleItems() {
        mPreferenceScreen.addPreference(createPreference(true));
        mPreferenceScreen.addPreference(createPreference(false));
        mPreferenceScreen.addPreference(createPreference(true));

        ArrayList<PreferenceStyle> styles = mController.generatePreferenceStyles();
        assertEquals(2, styles.size());
        assertEquals(mOuterRadius, styles.get(0).getTopRadius(), 0);
        assertEquals(mInnerRadius, styles.get(0).getBottomRadius(), 0);
        assertEquals(mInnerRadius, styles.get(1).getTopRadius(), 0);
        assertEquals(mOuterRadius, styles.get(1).getBottomRadius(), 0);
    }

    @Test
    @SmallTest
    public void testCustomStyledPreference_WithCustomMargins() {
        final int topMargin = 100;
        final int bottomMargin = 200;
        mPreferenceScreen.addPreference(
                createCustomTextMessagePreference(BackgroundStyle.CARD, topMargin, bottomMargin));

        ArrayList<PreferenceStyle> styles = mController.generatePreferenceStyles();
        assertEquals(1, styles.size());
        assertEquals(topMargin, styles.get(0).getTopMargin());
        assertEquals(bottomMargin, styles.get(0).getBottomMargin());
    }

    @Test
    @SmallTest
    public void testCustomStyledPreference_WithTopMarginOnly() {
        final int topMargin = 100;
        mPreferenceScreen.addPreference(
                createCustomTextMessagePreference(BackgroundStyle.CARD, topMargin, DEFAULT));

        ArrayList<PreferenceStyle> styles = mController.generatePreferenceStyles();
        assertEquals(1, styles.size());
        assertEquals(topMargin, styles.get(0).getTopMargin());
        assertEquals(DEFAULT, styles.get(0).getBottomMargin());
    }

    @Test
    @SmallTest
    public void testCustomStyledPreference_WithBottomMarginOnly() {
        final int bottomMargin = 200;
        mPreferenceScreen.addPreference(
                createCustomTextMessagePreference(BackgroundStyle.CARD, DEFAULT, bottomMargin));

        ArrayList<PreferenceStyle> styles = mController.generatePreferenceStyles();
        assertEquals(1, styles.size());
        assertEquals(DEFAULT, styles.get(0).getTopMargin());
        assertEquals(bottomMargin, styles.get(0).getBottomMargin());
    }

    @Test
    @SmallTest
    public void testCustomStyledPreference_WithBackgroundNone() {
        final int topMargin = 100;
        final int bottomMargin = 200;
        mPreferenceScreen.addPreference(
                createCustomTextMessagePreference(BackgroundStyle.NONE, topMargin, bottomMargin));

        ArrayList<PreferenceStyle> styles = mController.generatePreferenceStyles();
        assertEquals(1, styles.size());
        assertSame(PreferenceStyle.EMPTY, styles.get(0));
    }
}
