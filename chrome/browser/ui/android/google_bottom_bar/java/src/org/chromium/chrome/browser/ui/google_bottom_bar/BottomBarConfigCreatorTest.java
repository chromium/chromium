// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.ADD_NOTES;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.CUSTOM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.PIH_BASIC;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.REFRESH;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.SAVE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.SHARE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.DOUBLE_DECKER;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.NO_VARIANT;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.SINGLE_DECKER;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.GOOGLE_BOTTOM_BAR_NO_VARIANT_HEIGHT_DP_PARAM_VALUE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.GOOGLE_BOTTOM_BAR_SINGLE_DECKER_HEIGHT_DP_PARAM_VALUE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.GOOGLE_BOTTOM_BAR_VARIANT_LAYOUT_VALUE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED;

import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams.VariantLayoutType;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link BottomBarConfig}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class BottomBarConfigCreatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private CustomButtonParams mCustomButtonParams;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private Profile mProfile;

    private BottomBarConfigCreator mConfigCreator;
    private Context mContext;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mContext = activity);
        mConfigCreator = new BottomBarConfigCreator(mContext);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
    }

    @Test
    public void emptyList_returnsDefaultConfig() {
        GoogleBottomBarIntentParams params = GoogleBottomBarIntentParams.getDefaultInstance();

        assertDefaultConfig(mConfigCreator.create(params, List.of()), NO_VARIANT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void doubleDecker_emptyList_returnsDefaultConfig() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.DOUBLE_DECKER)
                        .build();

        assertDefaultConfig(mConfigCreator.create(params, List.of()), DOUBLE_DECKER);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void singleDecker_emptyList_returnsDefaultConfig() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();

        assertDefaultConfig(mConfigCreator.create(params, List.of()), SINGLE_DECKER);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void singleDeckerWithRightButtons_emptyList_returnsDefaultConfig() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                        .build();

        assertDefaultConfig(
                mConfigCreator.create(params, List.of()), SINGLE_DECKER_WITH_RIGHT_BUTTONS);
    }

    @Test
    public void onlyOneItem_returnsDefaultConfig() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder().addEncodedButton(1).build();

        assertDefaultConfig(mConfigCreator.create(params, List.of()), NO_VARIANT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void singleDeckerWithRightButtons_moreThanTwoButtons_returnsDefaultConfig() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                        .addAllEncodedButton(List.of(1, 1, 2, 3))
                        .build();

        assertDefaultConfig(
                mConfigCreator.create(params, List.of()), SINGLE_DECKER_WITH_RIGHT_BUTTONS);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            singleDeckerWithRightButtons_withSpotlightButtonInList_returnsConfigWithoutSpotlight() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                        .addAllEncodedButton(List.of(1, 1, 2))
                        .build();

        BottomBarConfig config = mConfigCreator.create(params, List.of());

        assertNull(config.getSpotlightId());
    }

    @Test
    public void invalidButtonIdInList_returnsDefaultConfig() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 15, 1))
                        .build();

        assertDefaultConfig(mConfigCreator.create(params, List.of()), NO_VARIANT);
    }

    @Test
    public void invalidSpotlightButton_returnsDefaultConfig() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(15, 1, 2, 3))
                        .build();

        assertDefaultConfig(mConfigCreator.create(params, List.of()), NO_VARIANT);
    }

    @Test
    public void noSpotlightParamList_nullSpotlight_correctButtonList() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .build();

        BottomBarConfig buttonConfig = mConfigCreator.create(params, List.of());

        assertNull(buttonConfig.getSpotlightId());
        assertEquals(3, buttonConfig.getButtonList().size());
        assertEquals(PIH_BASIC, buttonConfig.getButtonList().get(0).getId());
    }

    @Test
    public void withSpotlightParamList_correctSpotlightSet_correctButtonList() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(1, 1, 2, 3))
                        .build();

        BottomBarConfig buttonConfig = mConfigCreator.create(params, List.of());
        Integer spotlight = buttonConfig.getSpotlightId();

        assertNotNull(spotlight);
        assertEquals(spotlight.intValue(), PIH_BASIC);
        assertEquals(3, buttonConfig.getButtonList().size());
    }

    @Test
    public void createButtonConfigList_emptyCustomButtonParamsList() {
        List<Integer> buttonIdList = List.of(PIH_BASIC, PIH_BASIC, SHARE, SAVE, ADD_NOTES, REFRESH);
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder().addAllEncodedButton(buttonIdList).build();

        // empty customButtonParamsList - SAVE and ADD_NOTES are not included in the final list
        BottomBarConfig buttonConfig = mConfigCreator.create(params, new ArrayList<>());
        assertEquals(3, buttonConfig.getButtonList().size());
    }

    @Test
    public void createButtonConfigList_withCustomButtonParamsList() {
        Drawable drawable = mock(Drawable.class);
        when(mCustomButtonParams.getId()).thenReturn(100); // SAVE
        when(mCustomButtonParams.getIcon(mContext)).thenReturn(drawable);
        List<Integer> buttonIdList =
                List.of(
                        PIH_BASIC, PIH_BASIC, SHARE, SAVE, ADD_NOTES,
                        REFRESH); // PIH_BASIC, SHARE, SAVE, ADD_NOTES, REFRESH
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder().addAllEncodedButton(buttonIdList).build();

        // ADD_NOTES and REFRESH are not included in the final list as they are not supported
        BottomBarConfig buttonConfig = mConfigCreator.create(params, List.of(mCustomButtonParams));
        assertEquals(3, buttonConfig.getButtonList().size());
    }

    @Test
    public void createButtonConfigList_buttonIdListWithoutCustomParamId() {
        List<Integer> buttonIdList = List.of(PIH_BASIC, PIH_BASIC, SHARE); // PIH_BASIC, SHARE
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder().addAllEncodedButton(buttonIdList).build();

        when(mCustomButtonParams.getId()).thenReturn(100); // SAVE

        // SAVE is not included in the final list
        BottomBarConfig buttonConfig = mConfigCreator.create(params, List.of(mCustomButtonParams));
        assertEquals(2, buttonConfig.getButtonList().size());
    }

    @Test
    public void withCorrectCustomParams_hasCorrectButtonConfig() {
        Drawable drawable = mock(Drawable.class);
        when(mCustomButtonParams.getId()).thenReturn(100); // SAVE
        when(mCustomButtonParams.getIcon(mContext)).thenReturn(drawable);
        var pendingIntent = mock(PendingIntent.class);
        when(mCustomButtonParams.getPendingIntent()).thenReturn(pendingIntent);
        // PIH_BASIC, SHARE, SAVE, REFRESH
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(1, 1, 2, 3, 5))
                        .build();

        BottomBarConfig buttonConfig = mConfigCreator.create(params, List.of(mCustomButtonParams));

        // the button has the expected custom button params set
        assertEquals(pendingIntent, buttonConfig.getButtonList().get(2).getPendingIntent());
    }

    @Test
    public void hasPageInsightsCustomParams_usesChromePageInsightsIconInButtonConfig() {
        Drawable drawable = mock(Drawable.class);
        when(mCustomButtonParams.getId()).thenReturn(103); // PAGE INSIGHTS
        when(mCustomButtonParams.getIcon(mContext)).thenReturn(drawable);
        var pendingIntent = mock(PendingIntent.class);
        when(mCustomButtonParams.getPendingIntent()).thenReturn(pendingIntent);
        // PIH_BASIC, SHARE, SAVE
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .build();

        BottomBarConfig buttonConfig = mConfigCreator.create(params, List.of(mCustomButtonParams));

        Bitmap expectedBitmap =
                drawableToBitmap(
                        UiUtils.getTintedDrawable(
                                mContext,
                                R.drawable.bottom_bar_page_insights_icon,
                                R.color.default_icon_color_baseline));
        Bitmap actualBitmap = drawableToBitmap(buttonConfig.getButtonList().get(0).getIcon());
        // the button has the expected custom button params set
        assertTrue(expectedBitmap.sameAs(actualBitmap));
    }

    @Test
    public void hasSearchCustomParams_usesChromeSearchIconInButtonConfig() {
        Drawable drawable = mock(Drawable.class);
        when(mCustomButtonParams.getId()).thenReturn(106); // SEARCH
        when(mCustomButtonParams.getIcon(mContext)).thenReturn(drawable);
        var pendingIntent = mock(PendingIntent.class);
        when(mCustomButtonParams.getPendingIntent()).thenReturn(pendingIntent);
        // SEARCH, SHARE, SAVE
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 9, 2, 3))
                        .build();

        BottomBarConfig buttonConfig = mConfigCreator.create(params, List.of(mCustomButtonParams));

        Bitmap expectedBitmap =
                drawableToBitmap(
                        UiUtils.getTintedDrawable(
                                mContext,
                                R.drawable.ic_search,
                                R.color.default_icon_color_baseline));
        Bitmap actualBitmap = drawableToBitmap(buttonConfig.getButtonList().get(0).getIcon());
        // the button has the expected custom button params set
        assertTrue(expectedBitmap.sameAs(actualBitmap));
    }

    @Test
    public void hasHomeCustomParams_usesChromeHomeIconInButtonConfig() {
        Drawable drawable = mock(Drawable.class);
        when(mCustomButtonParams.getId()).thenReturn(107); // HOME
        when(mCustomButtonParams.getIcon(mContext)).thenReturn(drawable);
        var pendingIntent = mock(PendingIntent.class);
        when(mCustomButtonParams.getPendingIntent()).thenReturn(pendingIntent);
        // HOME, SHARE, SAVE
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 10, 2, 3))
                        .build();

        BottomBarConfig buttonConfig = mConfigCreator.create(params, List.of(mCustomButtonParams));

        Bitmap expectedBitmap =
                drawableToBitmap(
                        UiUtils.getTintedDrawable(
                                mContext,
                                R.drawable.bottom_bar_home_icon,
                                R.color.default_icon_color_baseline));
        Bitmap actualBitmap = drawableToBitmap(buttonConfig.getButtonList().get(0).getIcon());
        // the button has the expected custom button params set
        assertTrue(expectedBitmap.sameAs(actualBitmap));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void singleDeckerWithRightButtons_returnsCorrectConfig() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                        .addAllEncodedButton(List.of(0, SHARE))
                        .build();

        BottomBarConfig config = mConfigCreator.create(params, List.of());

        assertNull(config.getSpotlightId());
        assertEquals(1, config.getButtonList().size());
        assertEquals(SHARE, config.getButtonList().get(0).getId());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsDisabled_intentParamDoubleDecker_showsOnlyButtons() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.DOUBLE_DECKER)
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(NO_VARIANT, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsEnabled_intentParamNotSet_finchParamNotSet_showsOnlyButtons() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(NO_VARIANT, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsEnabled_defaultSearchEngineCheckDisabledAndNotEqualToGoogle_doubleDeckerLayoutSet_showsDoubleDeckerLayout() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.DOUBLE_DECKER)
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(DOUBLE_DECKER, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsEnabled_defaultSearchEngineCheckEnabledAndNotEqualToGoogle_doubleDeckerLayoutSet_showsNoVariantLayout() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.DOUBLE_DECKER)
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(NO_VARIANT, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsEnabled_intentParamChromeControlled_finchParamNotSet_returnsDoubleDecker() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.CHROME_CONTROLLED)
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(DOUBLE_DECKER, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsEnabled_intentParamChromeControlled_finchParamDoubleDecker_returnsDoubleDecker() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .setVariantLayoutType(VariantLayoutType.CHROME_CONTROLLED)
                        .build();
        GOOGLE_BOTTOM_BAR_VARIANT_LAYOUT_VALUE.setForTesting(DOUBLE_DECKER);

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(DOUBLE_DECKER, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsEnabled_intentParamChromeControlled_finchParamSingleDecker_returnsSingleDecker() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .setVariantLayoutType(VariantLayoutType.CHROME_CONTROLLED)
                        .build();
        GOOGLE_BOTTOM_BAR_VARIANT_LAYOUT_VALUE.setForTesting(SINGLE_DECKER);

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(SINGLE_DECKER, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsEnabled_intentParamChromeControlled_finchParamSingleDeckerWithRightButtons_returnsSingleDeckerWithRightButtons() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .setVariantLayoutType(VariantLayoutType.CHROME_CONTROLLED)
                        .build();
        GOOGLE_BOTTOM_BAR_VARIANT_LAYOUT_VALUE.setForTesting(SINGLE_DECKER_WITH_RIGHT_BUTTONS);

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(SINGLE_DECKER_WITH_RIGHT_BUTTONS, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsEnabled_intentParamDoubleDecker_showsDoubleDecker() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .setVariantLayoutType(VariantLayoutType.DOUBLE_DECKER)
                        .build();
        GOOGLE_BOTTOM_BAR_VARIANT_LAYOUT_VALUE.setForTesting(NO_VARIANT);

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(DOUBLE_DECKER, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsEnabled_intentParamSingleDecker_showsSingleDecker() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();
        GOOGLE_BOTTOM_BAR_VARIANT_LAYOUT_VALUE.setForTesting(NO_VARIANT);

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(SINGLE_DECKER, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getVariantLayoutType_variantLayoutsEnabled_intentParamSingleDeckerWithRightButtons_showsSingleDeckerWithRightButtons() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 1, 2, 3))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();
        GOOGLE_BOTTOM_BAR_VARIANT_LAYOUT_VALUE.setForTesting(NO_VARIANT);

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(SINGLE_DECKER, bottomBarConfig.getVariantLayoutType());
    }

    @Test
    public void create_noVariant_heightNotSpecified_hasDefaultHeight() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.NO_VARIANT)
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(
                BottomBarConfigCreator.DEFAULT_NO_VARIANT_HEIGHT_DP, bottomBarConfig.getHeightDp());
    }

    @Test
    public void create_noVariant_heightSpecifiedByFinch_hasFinchHeight() {
        GOOGLE_BOTTOM_BAR_NO_VARIANT_HEIGHT_DP_PARAM_VALUE.setForTesting(123);
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.NO_VARIANT)
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(123, bottomBarConfig.getHeightDp());
    }

    @Test
    public void create_noVariant_heightSpecifiedByIntent_hasIntentHeight() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.NO_VARIANT)
                        .setNoVariantHeightDp(123)
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(123, bottomBarConfig.getHeightDp());
    }

    @Test
    public void create_noVariant_heightSpecifiedByFinchAndIntent_hasIntentHeight() {
        GOOGLE_BOTTOM_BAR_NO_VARIANT_HEIGHT_DP_PARAM_VALUE.setForTesting(123);
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.NO_VARIANT)
                        .setNoVariantHeightDp(456)
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(456, bottomBarConfig.getHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void create_singleDecker_withEmptyList_hasCorrectBottomBarConfig() {
        Drawable drawable = mock(Drawable.class);
        when(mCustomButtonParams.getId()).thenReturn(105); // CUSTOM
        when(mCustomButtonParams.getIcon(mContext)).thenReturn(drawable);
        var pendingIntent = mock(PendingIntent.class);
        when(mCustomButtonParams.getPendingIntent()).thenReturn(pendingIntent);
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();

        BottomBarConfig bottomBarConfig =
                mConfigCreator.create(params, List.of(mCustomButtonParams));

        assertEquals(SINGLE_DECKER, bottomBarConfig.getVariantLayoutType());
        assertEquals(0, bottomBarConfig.getButtonList().size());
        assertNull(bottomBarConfig.getSpotlightId());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void create_singleDecker_heightNotSpecified_hasDefaultHeight() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(
                BottomBarConfigCreator.DEFAULT_SINGLE_DECKER_HEIGHT_DP,
                bottomBarConfig.getHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void create_singleDecker_heightSpecifiedByFinch_hasFinchHeight() {
        GOOGLE_BOTTOM_BAR_SINGLE_DECKER_HEIGHT_DP_PARAM_VALUE.setForTesting(123);
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(123, bottomBarConfig.getHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void create_singleDecker_heightSpecifiedByIntent_hasIntentHeight() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .setSingleDeckerHeightDp(123)
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(123, bottomBarConfig.getHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void create_singleDecker_heightSpecifiedByFinchAndIntent_hasIntentHeight() {
        GOOGLE_BOTTOM_BAR_SINGLE_DECKER_HEIGHT_DP_PARAM_VALUE.setForTesting(123);
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .setSingleDeckerHeightDp(456)
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(456, bottomBarConfig.getHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void create_withEmptyList_singleDeckerWithRightButtons_hasCorrectBottomBarConfig() {
        GoogleBottomBarIntentParams params =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, 1))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();

        BottomBarConfig bottomBarConfig = mConfigCreator.create(params, List.of());

        assertEquals(SINGLE_DECKER, bottomBarConfig.getVariantLayoutType());
        assertEquals(0, bottomBarConfig.getButtonList().size());
        assertNull(bottomBarConfig.getSpotlightId());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void initDefaultSearchEngine_variantLayoutsDisabled_doesNotCheckDefaultSearchEngine() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);

        BottomBarConfigCreator.initDefaultSearchEngine(mProfile);

        verifyNoInteractions(mTemplateUrlService);
        // Verify that true value is not written to Shared preferences
        // Use true as default value as we expect to get false
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, /* defaultValue= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            initDefaultSearchEngine_variantLayoutsEnabled_defaultSearchEngineCheckEnabled_doesNotCheckDefaultSearchEngine() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);

        BottomBarConfigCreator.initDefaultSearchEngine(mProfile);

        verifyNoInteractions(mTemplateUrlService);
        // Verify that true value is not written to Shared preferences
        // Use true as default value as we expect to get false
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, /* defaultValue= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            initDefaultSearchEngine_variantLayoutsEnabled_defaultSearchEngineCheckEnabledAndNotEqualToGoogle_writesFalse() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, true);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);

        BottomBarConfigCreator.initDefaultSearchEngine(mProfile);

        // Use true as default value as we expect to get false
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, /* defaultValue= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            initDefaultSearchEngine_variantLayoutsEnabled_defaultSearchEngineCheckEnabledAndEqualsToGoogle_writesTrue() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);

        BottomBarConfigCreator.initDefaultSearchEngine(mProfile);

        // Use false as default value as we expect to get true
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, /* defaultValue= */ false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsDisabled_returnsSetWithAllSupportedCustomParams() {
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.NO_VARIANT)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                BottomBarConfigCreator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(
                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet(), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabled_noVariantLayout_returnsSetWithAllSupportedCustomParams() {
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.NO_VARIANT)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                BottomBarConfigCreator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(
                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet(), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabled_doubleDecker_returnsSetWithAllSupportedCustomParams() {
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.DOUBLE_DECKER)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                BottomBarConfigCreator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(
                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet(), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabled_singleDecker_returnsEmptySet() {
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                BottomBarConfigCreator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(Set.of(), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabledButNotSupported_singleDecker_returnsSetWithAllSupportedCustomParams() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                BottomBarConfigCreator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(
                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet(), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabled_singleDeckerWithButtonsOnRight_returnsSetWithEncodedButtons() {
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                BottomBarConfigCreator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(Set.of(100, 105), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabledButNotSupported_singleDeckerWithButtonsOnRight_returnsSetWithAllSupportedCustomParams() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                BottomBarConfigCreator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(
                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet(), supportedGoogleBottomBarButtons);
    }

    private static Bitmap drawableToBitmap(Drawable drawable) {
        Bitmap bitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }

    private static void assertDefaultConfig(
            BottomBarConfig config, @GoogleBottomBarVariantLayoutType int layoutType) {
        assertNull(config.getSpotlightId());

        if (layoutType == NO_VARIANT) {
            assertEquals(2, config.getButtonList().size());
            assertEquals(SAVE, config.getButtonList().get(0).getId());
            assertEquals(SHARE, config.getButtonList().get(1).getId());
            assertEquals(BottomBarConfigCreator.DEFAULT_NO_VARIANT_HEIGHT_DP, config.getHeightDp());
        } else if (layoutType == DOUBLE_DECKER) {
            assertEquals(2, config.getButtonList().size());
            assertEquals(SAVE, config.getButtonList().get(0).getId());
            assertEquals(SHARE, config.getButtonList().get(1).getId());
            assertEquals(BottomBarConfigCreator.DOUBLE_DECKER_HEIGHT_DP, config.getHeightDp());
        } else if (layoutType == SINGLE_DECKER_WITH_RIGHT_BUTTONS) {
            assertEquals(1, config.getButtonList().size());
            assertEquals(SHARE, config.getButtonList().get(0).getId());
            assertEquals(
                    BottomBarConfigCreator.SINGLE_DECKER_WITH_RIGHT_BUTTONS_HEIGHT_DP,
                    config.getHeightDp());
        } else if (layoutType == SINGLE_DECKER) {
            assertEquals(0, config.getButtonList().size());
            assertEquals(
                    BottomBarConfigCreator.DEFAULT_SINGLE_DECKER_HEIGHT_DP, config.getHeightDp());
        } else {
            throw new IllegalArgumentException(
                    String.format("Layout type with id %s is not tested.", layoutType));
        }
        assertEquals(config.getVariantLayoutType(), layoutType);
    }
}
