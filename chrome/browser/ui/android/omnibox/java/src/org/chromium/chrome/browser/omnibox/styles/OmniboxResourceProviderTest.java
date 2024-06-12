// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Drawable.ConstantState;

import androidx.annotation.ColorInt;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import com.google.android.material.color.MaterialColors;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.TestActivity;

/** Tests for {@link OmniboxResourceProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxResourceProviderTest {
    private static final String TAG = "ORPTest";

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;

    private @ColorInt int mDefaultColor;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        mDefaultColor = ChromeColors.getDefaultThemeColor(mActivity, false);
    }

    @Test
    public void resolveAttributeToDrawable() {
        Drawable drawableLight =
                OmniboxResourceProvider.resolveAttributeToDrawable(
                        mActivity,
                        BrandedColorScheme.LIGHT_BRANDED_THEME,
                        R.attr.selectableItemBackground);
        Assert.assertNotNull(drawableLight);

        Drawable drawableDark =
                OmniboxResourceProvider.resolveAttributeToDrawable(
                        mActivity,
                        BrandedColorScheme.DARK_BRANDED_THEME,
                        R.attr.selectableItemBackground);
        Assert.assertNotNull(drawableDark);
    }

    @Test
    public void getColorScheme_incognito() {
        assertEquals(
                "Color scheme should be INCOGNITO.",
                BrandedColorScheme.INCOGNITO,
                OmniboxResourceProvider.getBrandedColorScheme(mActivity, true, mDefaultColor));
        assertEquals(
                "Color scheme should be INCOGNITO.",
                BrandedColorScheme.INCOGNITO,
                OmniboxResourceProvider.getBrandedColorScheme(mActivity, true, Color.RED));
    }

    @Test
    public void getColorScheme_nonIncognito() {
        assertEquals(
                "Color scheme should be DEFAULT.",
                BrandedColorScheme.APP_DEFAULT,
                OmniboxResourceProvider.getBrandedColorScheme(mActivity, false, mDefaultColor));
        assertEquals(
                "Color scheme should be DARK_THEME.",
                BrandedColorScheme.DARK_BRANDED_THEME,
                OmniboxResourceProvider.getBrandedColorScheme(mActivity, false, Color.BLACK));
        assertEquals(
                "Color scheme should be LIGHT_THEME.",
                BrandedColorScheme.LIGHT_BRANDED_THEME,
                OmniboxResourceProvider.getBrandedColorScheme(
                        mActivity, false, Color.parseColor("#eaecf0" /*Light grey color*/)));
    }

    @Test
    public void getUrlBarPrimaryTextColor() {
        final int darkTextColor = mActivity.getColor(R.color.branded_url_text_on_light_bg);
        final int lightTextColor = mActivity.getColor(R.color.branded_url_text_on_dark_bg);
        final int incognitoColor = mActivity.getColor(R.color.url_bar_primary_text_incognito);
        final int defaultColor = MaterialColors.getColor(mActivity, R.attr.colorOnSurface, TAG);

        assertEquals(
                "Wrong url bar primary text color for LIGHT_THEME.",
                darkTextColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong url bar primary text color for DARK_THEME.",
                lightTextColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong url bar primary text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong url bar primary text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getUrlBarSecondaryTextColor() {
        final int darkTextColor = mActivity.getColor(R.color.branded_url_text_variant_on_light_bg);
        final int lightTextColor = mActivity.getColor(R.color.branded_url_text_variant_on_dark_bg);
        final int incognitoColor = mActivity.getColor(R.color.url_bar_secondary_text_incognito);
        final int defaultColor =
                MaterialColors.getColor(mActivity, R.attr.colorOnSurfaceVariant, TAG);

        assertEquals(
                "Wrong url bar secondary text color for LIGHT_THEME.",
                darkTextColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong url bar secondary text color for DARK_THEME.",
                lightTextColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong url bar secondary text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mActivity, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong url bar secondary text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getUrlBarDangerColor() {
        final int redOnDark = mActivity.getColor(R.color.default_red_light);
        final int redOnLight = mActivity.getColor(R.color.default_red_dark);

        assertEquals(
                "Danger color for DARK_THEME should be the lighter red.",
                redOnDark,
                OmniboxResourceProvider.getUrlBarDangerColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Danger color for LIGHT_THEME should be the darker red.",
                redOnLight,
                OmniboxResourceProvider.getUrlBarDangerColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Danger color for DEFAULT should be the darker red when we're in light theme.",
                redOnLight,
                OmniboxResourceProvider.getUrlBarDangerColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT));
        assertEquals(
                "Danger color for INCOGNITO should be the lighter red.",
                redOnDark,
                OmniboxResourceProvider.getUrlBarDangerColor(
                        mActivity, BrandedColorScheme.INCOGNITO));
    }

    @Test
    public void getUrlBarSecureColor() {
        final int greenOnDark = mActivity.getColor(R.color.default_green_light);
        final int greenOnLight = mActivity.getColor(R.color.default_green_dark);

        assertEquals(
                "Secure color for DARK_THEME should be the lighter green.",
                greenOnDark,
                OmniboxResourceProvider.getUrlBarSecureColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Secure color for LIGHT_THEME should be the darker green.",
                greenOnLight,
                OmniboxResourceProvider.getUrlBarSecureColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Secure color for DEFAULT should be the darker green when we're in light theme.",
                greenOnLight,
                OmniboxResourceProvider.getUrlBarSecureColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT));
        assertEquals(
                "Secure color for INCOGNITO should be the lighter green.",
                greenOnDark,
                OmniboxResourceProvider.getUrlBarSecureColor(
                        mActivity, BrandedColorScheme.INCOGNITO));
    }

    @Test
    public void getSuggestionPrimaryTextColor() {
        final int incognitoColor = mActivity.getColor(R.color.default_text_color_light);
        final int defaultColor = MaterialColors.getColor(mActivity, R.attr.colorOnSurface, TAG);

        assertEquals(
                "Wrong suggestion primary text color for LIGHT_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion primary text color for DARK_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion primary text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mActivity, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong suggestion primary text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getSuggestionSecondaryTextColor() {
        final int incognitoColor = mActivity.getColor(R.color.default_text_color_secondary_light);
        final int defaultColor =
                MaterialColors.getColor(mActivity, R.attr.colorOnSurfaceVariant, TAG);

        assertEquals(
                "Wrong suggestion secondary text color for LIGHT_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion secondary text color for DARK_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion secondary text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mActivity, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong suggestion secondary text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getSuggestionUrlTextColor() {
        final int incognitoColor = mActivity.getColor(R.color.suggestion_url_color_incognito);
        final int defaultColor = SemanticColorUtils.getDefaultTextColorLink(mActivity);

        assertEquals(
                "Wrong suggestion url text color for LIGHT_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion url text color for DARK_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion url text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mActivity, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong suggestion url text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getStatusSeparatorColor() {
        final int darkColor = mActivity.getColor(R.color.locationbar_status_separator_color_dark);
        final int lightColor = mActivity.getColor(R.color.locationbar_status_separator_color_light);
        final int incognitoColor =
                mActivity.getColor(R.color.locationbar_status_separator_color_incognito);
        final int defaultColor = MaterialColors.getColor(mActivity, R.attr.colorOutline, TAG);

        assertEquals(
                "Wrong status separator color for LIGHT_THEME.",
                darkColor,
                OmniboxResourceProvider.getStatusSeparatorColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong status separator color for DARK_THEME.",
                lightColor,
                OmniboxResourceProvider.getStatusSeparatorColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong status separator color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getStatusSeparatorColor(
                        mActivity, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong status separator color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getStatusSeparatorColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getStatusPreviewTextColor() {
        final int darkColor = mActivity.getColor(R.color.locationbar_status_preview_color_dark);
        final int lightColor = mActivity.getColor(R.color.locationbar_status_preview_color_light);
        final int incognitoColor =
                mActivity.getColor(R.color.locationbar_status_preview_color_incognito);
        final int defaultColor = MaterialColors.getColor(mActivity, R.attr.colorPrimary, TAG);

        assertEquals(
                "Wrong status preview text color for LIGHT_THEME.",
                darkColor,
                OmniboxResourceProvider.getStatusPreviewTextColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong status preview text color for DARK_THEME.",
                lightColor,
                OmniboxResourceProvider.getStatusPreviewTextColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong status preview text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getStatusPreviewTextColor(
                        mActivity, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong status preview text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getStatusPreviewTextColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getStatusOfflineTextColor() {
        final int darkColor = mActivity.getColor(R.color.locationbar_status_offline_color_dark);
        final int lightColor = mActivity.getColor(R.color.locationbar_status_offline_color_light);
        final int incognitoColor =
                mActivity.getColor(R.color.locationbar_status_offline_color_incognito);
        final int defaultColor =
                MaterialColors.getColor(mActivity, R.attr.colorOnSurfaceVariant, TAG);

        assertEquals(
                "Wrong status offline text color for LIGHT_THEME.",
                darkColor,
                OmniboxResourceProvider.getStatusOfflineTextColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong status offline text color for DARK_THEME.",
                lightColor,
                OmniboxResourceProvider.getStatusOfflineTextColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong status offline text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getStatusOfflineTextColor(
                        mActivity, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong status offline text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getStatusOfflineTextColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_CACHE_SUGGESTION_RESOURCES)
    public void getDrawableCached() {
        Drawable drawable =
                OmniboxResourceProvider.getDrawable(mActivity, R.drawable.btn_suggestion_refine);
        ConstantState constantState = drawable.getConstantState();

        Assert.assertEquals(
                constantState,
                OmniboxResourceProvider.getDrawableCacheForTesting()
                        .get(R.drawable.btn_suggestion_refine));

        drawable = OmniboxResourceProvider.getDrawable(mActivity, R.drawable.btn_suggestion_refine);
        Assert.assertNotNull(drawable);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_CACHE_SUGGESTION_RESOURCES)
    public void getStringCached() {
        String refineString =
                OmniboxResourceProvider.getString(
                        mActivity, R.string.accessibility_omnibox_btn_refine, "foobar");

        Assert.assertEquals(
                mActivity.getString(R.string.accessibility_omnibox_btn_refine, "foobar"),
                refineString);
        Assert.assertEquals(
                mActivity.getString(R.string.accessibility_omnibox_btn_refine),
                OmniboxResourceProvider.getStringCacheForTesting()
                        .get(R.string.accessibility_omnibox_btn_refine));

        String copyString = OmniboxResourceProvider.getString(mActivity, R.string.copy_link);
        Assert.assertEquals(mActivity.getString(R.string.copy_link, "foobar"), copyString);
        Assert.assertEquals(
                copyString,
                OmniboxResourceProvider.getStringCacheForTesting().get(R.string.copy_link));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_CACHE_SUGGESTION_RESOURCES)
    public void invalidateDrawableCache() {
        Drawable drawable =
                OmniboxResourceProvider.getDrawable(mActivity, R.drawable.btn_suggestion_refine);
        ConstantState constantState = drawable.getConstantState();

        Assert.assertEquals(
                constantState,
                OmniboxResourceProvider.getDrawableCacheForTesting()
                        .get(R.drawable.btn_suggestion_refine));

        OmniboxResourceProvider.invalidateDrawableCache();
        Assert.assertEquals(0, OmniboxResourceProvider.getDrawableCacheForTesting().size());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void replaceContextForSmallTabletWindow() {
        Context originalContext = mActivity;
        originalContext.getResources().getConfiguration().screenWidthDp = 700;
        Assert.assertEquals(
                originalContext,
                OmniboxResourceProvider.maybeReplaceContextForSmallTabletWindow(originalContext));

        originalContext.getResources().getConfiguration().screenWidthDp = 400;
        Assert.assertNotEquals(
                originalContext,
                OmniboxResourceProvider.maybeReplaceContextForSmallTabletWindow(originalContext));
    }

    @Test
    public void getAdditionalTextColor() {
        final int defaultTextColorSecondary =
                MaterialColors.getColor(mActivity, R.attr.colorOnSurfaceVariant, TAG);
        assertEquals(
                "Wrong additional text color.",
                defaultTextColorSecondary,
                OmniboxResourceProvider.getAdditionalTextColor(mActivity));
    }
}
