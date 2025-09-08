// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertSame;

import static org.chromium.components.browser_ui.settings.CustomStyledPreference.DEFAULT_COLOR;
import static org.chromium.components.browser_ui.settings.CustomStyledPreference.DEFAULT_MARGIN;

import android.content.Context;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.CustomStyledPreference.BackgroundStyle;
import org.chromium.components.browser_ui.settings.test.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link SettingsStylingController}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SettingsStylingControllerTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private int mDefaultRadius;
    private int mInnerRadius;
    private int mSectionBottomMargin;
    private int mVerticalMargin;
    private int mHorizontalMargin;
    private int mBackgroundColor;

    private static final int CUSTOM_TOP_MARGIN = 100;
    private static final int CUSTOM_BOTTOM_MARGIN = 200;
    private static final int CUSTOM_HORIZONTAL_MARGIN = 50;

    private Context mContext;
    private SettingsStylingController mController;
    private PreferenceScreen mPreferenceScreen;
    private List<Preference> mVisiblePreferences;
    private ArrayList<PreferenceStyle> mPreferenceStyles;

    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mContext = mSettingsRule.getActivity();
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
        mController = new SettingsStylingController(mContext, mPreferenceScreen);
        mDefaultRadius =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_default);
        mInnerRadius =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_inner);
        mSectionBottomMargin =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_section_bottom_margin);
        mVerticalMargin =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_vertical_margin);
        mHorizontalMargin =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_horizontal_margin);
        mBackgroundColor = SemanticColorUtils.getColorSurfaceContainerLowest(mContext);

        mSettingsRule
                .getPreferenceFragment()
                .addPreferencesFromResource(R.xml.test_settings_custom_preference_screen);

        // Manually add preferences with custom margins
        addPreferencesWithCustomMargins();

        mVisiblePreferences = new ArrayList<>();
        for (int i = 0; i < mPreferenceScreen.getPreferenceCount(); i++) {
            Preference p = mPreferenceScreen.getPreference(i);
            if (p.isVisible()) {
                mVisiblePreferences.add(p);
            }
        }
        mPreferenceStyles = mController.generatePreferenceStyles();
    }

    @Test
    @SmallTest
    public void testPreferenceStyleCount() {
        assertEquals(
                "The number of styles should match the number of visible preferences.",
                mVisiblePreferences.size(),
                mPreferenceStyles.size());
    }

    @Test
    @SmallTest
    public void testPreferenceCategoryStyle() {
        PreferenceStyle preferenceCategoryStyle = getPreferenceStyle("preference_category");
        assertSame(PreferenceStyle.EMPTY, preferenceCategoryStyle);
    }

    @Test
    @SmallTest
    public void testTextMessagePreferenceStyle() {
        PreferenceStyle textMessagePreferencestyle = getPreferenceStyle("text_message_preference");
        assertSame(PreferenceStyle.EMPTY, textMessagePreferencestyle);
    }

    @Test
    @SmallTest
    public void testPreferenceStyle_PreferenceTop() {
        PreferenceStyle preferenceTopStyle = getPreferenceStyle("preference_top");
        assertEquals(mDefaultRadius, preferenceTopStyle.getTopRadius(), 0);
        assertEquals(mInnerRadius, preferenceTopStyle.getBottomRadius(), 0);
        assertEquals(mVerticalMargin, preferenceTopStyle.getTopMargin());
        assertEquals(mVerticalMargin, preferenceTopStyle.getBottomMargin());
    }

    @Test
    @SmallTest
    public void testPreferenceStyle_PreferenceMiddle() {
        PreferenceStyle preferenceMiddleStyle = getPreferenceStyle("preference_middle");
        assertEquals(mInnerRadius, preferenceMiddleStyle.getTopRadius(), 0);
        assertEquals(mInnerRadius, preferenceMiddleStyle.getBottomRadius(), 0);
        assertEquals(mVerticalMargin, preferenceMiddleStyle.getTopMargin());
        assertEquals(mVerticalMargin, preferenceMiddleStyle.getBottomMargin());
    }

    @Test
    @SmallTest
    public void testPreferenceStyle_PreferenceBottom() {
        PreferenceStyle preferenceBottomStyle = getPreferenceStyle("preference_bottom");
        assertEquals(mInnerRadius, preferenceBottomStyle.getTopRadius(), 0);
        assertEquals(mDefaultRadius, preferenceBottomStyle.getBottomRadius(), 0);
        assertEquals(mVerticalMargin, preferenceBottomStyle.getTopMargin());
        assertEquals(
                mVerticalMargin + mSectionBottomMargin, preferenceBottomStyle.getBottomMargin());
    }

    @Test
    @SmallTest
    public void testCardBackgroundStyle() {
        PreferenceStyle styleCard = getPreferenceStyle("preference_card");
        assertEquals(mDefaultRadius, styleCard.getTopRadius(), 0);
        assertEquals(mDefaultRadius, styleCard.getBottomRadius(), 0);
        assertEquals(mVerticalMargin, styleCard.getTopMargin());
        assertEquals(mVerticalMargin + mSectionBottomMargin, styleCard.getBottomMargin());
        assertEquals(mBackgroundColor, styleCard.getBackgroundColor());
    }

    @Test
    @SmallTest
    public void testCustomColorBackgroundStyle() {
        PreferenceStyle styleCustomColor = getPreferenceStyle("preference_color");
        assertEquals(mDefaultRadius, styleCustomColor.getTopRadius(), 0);
        assertEquals(mDefaultRadius, styleCustomColor.getBottomRadius(), 0);
        assertEquals(mVerticalMargin, styleCustomColor.getTopMargin());
        assertEquals(mVerticalMargin + mSectionBottomMargin, styleCustomColor.getBottomMargin());
        assertEquals(
                mContext.getColor(android.R.color.holo_blue_light),
                styleCustomColor.getBackgroundColor());
    }

    @Test
    @SmallTest
    public void testStandardBackgroundStyle() {
        PreferenceStyle styleStandard = getPreferenceStyle("preference_standard");
        assertEquals(mDefaultRadius, styleStandard.getTopRadius(), 0);
        assertEquals(mDefaultRadius, styleStandard.getBottomRadius(), 0);
        assertEquals(mVerticalMargin, styleStandard.getTopMargin());
        assertEquals(mVerticalMargin + mSectionBottomMargin, styleStandard.getBottomMargin());
        assertEquals(mBackgroundColor, styleStandard.getBackgroundColor());
    }

    @Test
    @SmallTest
    public void testNoneBackgroundStyle() {
        PreferenceStyle styleNone = getPreferenceStyle("preference_none");
        assertSame(PreferenceStyle.EMPTY, styleNone);
    }

    @Test
    @SmallTest
    public void testCustomStyledPreference_WithCustomMargins() {
        PreferenceStyle customMarginPreferenceStyle =
                getPreferenceStyle("preference_with_custom_margins");
        assertEquals(CUSTOM_TOP_MARGIN, customMarginPreferenceStyle.getTopMargin());
        assertEquals(CUSTOM_BOTTOM_MARGIN, customMarginPreferenceStyle.getBottomMargin());
        assertEquals(mHorizontalMargin, customMarginPreferenceStyle.getHorizontalMargin());
    }

    @Test
    @SmallTest
    public void testCustomStyledPreference_WithTopMarginOnly() {
        PreferenceStyle topMarginOnlyPreferenceStyle =
                getPreferenceStyle("preference_with_top_margin_only");
        assertEquals(CUSTOM_TOP_MARGIN, topMarginOnlyPreferenceStyle.getTopMargin());
        assertEquals(
                mVerticalMargin + mSectionBottomMargin,
                topMarginOnlyPreferenceStyle.getBottomMargin());
        assertEquals(mHorizontalMargin, topMarginOnlyPreferenceStyle.getHorizontalMargin());
    }

    @Test
    @SmallTest
    public void testCustomStyledPreference_WithBottomMarginOnly() {
        PreferenceStyle bottomMarginOnlyPreferenceStyle =
                getPreferenceStyle("preference_with_bottom_margin_only");
        assertEquals(mVerticalMargin, bottomMarginOnlyPreferenceStyle.getTopMargin());
        assertEquals(CUSTOM_BOTTOM_MARGIN, bottomMarginOnlyPreferenceStyle.getBottomMargin());
        assertEquals(mHorizontalMargin, bottomMarginOnlyPreferenceStyle.getHorizontalMargin());
    }

    @Test
    @SmallTest
    public void testCustomStyledPreference_WithBottomAndHorizontalMargin() {
        PreferenceStyle bottomAndHorizontalMarginsPreferenceStyle =
                getPreferenceStyle("preference_with_bottom_and_horizontal_margins");
        assertEquals(mVerticalMargin, bottomAndHorizontalMarginsPreferenceStyle.getTopMargin());
        assertEquals(
                CUSTOM_BOTTOM_MARGIN, bottomAndHorizontalMarginsPreferenceStyle.getBottomMargin());
        assertEquals(
                CUSTOM_HORIZONTAL_MARGIN,
                bottomAndHorizontalMarginsPreferenceStyle.getHorizontalMargin());
    }

    private ChromeBasePreference createCustomPreference(
            @BackgroundStyle int backgroundStyle,
            int topMargin,
            int bottomMargin,
            int horizontalMargin,
            int backgroundColor) {
        return new ChromeBasePreference(mContext, null) {
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

            @Override
            public int getCustomHorizontalMargin() {
                return horizontalMargin;
            }

            @Override
            public int getCustomBackgroundColor() {
                return backgroundColor;
            }
        };
    }

    private void addPreferencesWithCustomMargins() {
        // Scenario 1: Custom top and bottom margins
        ChromeBasePreference customMarginsPreference =
                createCustomPreference(
                        BackgroundStyle.CARD,
                        CUSTOM_TOP_MARGIN,
                        CUSTOM_BOTTOM_MARGIN,
                        DEFAULT_MARGIN,
                        DEFAULT_COLOR);
        customMarginsPreference.setKey("preference_with_custom_margins");
        mPreferenceScreen.addPreference(customMarginsPreference);

        // Scenario 2: Custom top margin only
        ChromeBasePreference topMarginOnlyPreference =
                createCustomPreference(
                        BackgroundStyle.CARD,
                        CUSTOM_TOP_MARGIN,
                        DEFAULT_MARGIN,
                        DEFAULT_MARGIN,
                        DEFAULT_COLOR);
        topMarginOnlyPreference.setKey("preference_with_top_margin_only");
        mPreferenceScreen.addPreference(topMarginOnlyPreference);

        // Scenario 3: Custom bottom margin only
        ChromeBasePreference bottomMarginOnlyPreference =
                createCustomPreference(
                        BackgroundStyle.CARD,
                        DEFAULT_MARGIN,
                        CUSTOM_BOTTOM_MARGIN,
                        DEFAULT_MARGIN,
                        DEFAULT_COLOR);
        bottomMarginOnlyPreference.setKey("preference_with_bottom_margin_only");
        mPreferenceScreen.addPreference(bottomMarginOnlyPreference);

        // Scenario 4: Custom bottom and horizontal margins
        ChromeBasePreference bottomAndHorizontalMarginsPreference =
                createCustomPreference(
                        BackgroundStyle.CARD,
                        DEFAULT_MARGIN,
                        CUSTOM_BOTTOM_MARGIN,
                        CUSTOM_HORIZONTAL_MARGIN,
                        DEFAULT_COLOR);
        bottomAndHorizontalMarginsPreference.setKey(
                "preference_with_bottom_and_horizontal_margins");
        mPreferenceScreen.addPreference(bottomAndHorizontalMarginsPreference);
    }

    private PreferenceStyle getPreferenceStyle(String key) {
        Preference preference = mPreferenceScreen.findPreference(key);
        int preferenceIndex = mVisiblePreferences.indexOf(preference);
        assertNotEquals(
                "Preference '" + key + "' not found in visible preferences.", -1, preferenceIndex);
        return mPreferenceStyles.get(preferenceIndex);
    }
}
