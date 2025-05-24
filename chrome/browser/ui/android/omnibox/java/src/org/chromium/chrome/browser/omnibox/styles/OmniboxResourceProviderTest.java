// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Drawable.ConstantState;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;

import com.google.android.material.color.MaterialColors;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Tests for {@link OmniboxResourceProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE
})
public class OmniboxResourceProviderTest {
    private static final String TAG = "ORPTest";

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @ColorInt int mDefaultColor;
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                spy(
                        new ContextThemeWrapper(
                                ContextUtils.getApplicationContext(),
                                R.style.Theme_BrowserUI_DayNight));
        mDefaultColor = ChromeColors.getDefaultThemeColor(mContext, false);
    }

    @Test
    public void resolveAttributeToDrawable() {
        Drawable drawableLight =
                OmniboxResourceProvider.resolveAttributeToDrawable(
                        mContext,
                        BrandedColorScheme.LIGHT_BRANDED_THEME,
                        R.attr.selectableItemBackground);
        Assert.assertNotNull(drawableLight);

        Drawable drawableDark =
                OmniboxResourceProvider.resolveAttributeToDrawable(
                        mContext,
                        BrandedColorScheme.DARK_BRANDED_THEME,
                        R.attr.selectableItemBackground);
        Assert.assertNotNull(drawableDark);
    }

    @Test
    public void getColorScheme_incognito() {
        assertEquals(
                "Color scheme should be INCOGNITO.",
                BrandedColorScheme.INCOGNITO,
                OmniboxResourceProvider.getBrandedColorScheme(mContext, true, mDefaultColor));
        assertEquals(
                "Color scheme should be INCOGNITO.",
                BrandedColorScheme.INCOGNITO,
                OmniboxResourceProvider.getBrandedColorScheme(mContext, true, Color.RED));
    }

    @Test
    public void getColorScheme_nonIncognito() {
        assertEquals(
                "Color scheme should be DEFAULT.",
                BrandedColorScheme.APP_DEFAULT,
                OmniboxResourceProvider.getBrandedColorScheme(mContext, false, mDefaultColor));
        assertEquals(
                "Color scheme should be DARK_THEME.",
                BrandedColorScheme.DARK_BRANDED_THEME,
                OmniboxResourceProvider.getBrandedColorScheme(mContext, false, Color.BLACK));
        assertEquals(
                "Color scheme should be LIGHT_THEME.",
                BrandedColorScheme.LIGHT_BRANDED_THEME,
                OmniboxResourceProvider.getBrandedColorScheme(
                        mContext, false, Color.parseColor("#eaecf0" /*Light grey color*/)));
    }

    @Test
    public void getUrlBarPrimaryTextColor() {
        final int darkTextColor = mContext.getColor(R.color.branded_url_text_on_light_bg);
        final int lightTextColor = mContext.getColor(R.color.branded_url_text_on_dark_bg);
        final int incognitoColor = mContext.getColor(R.color.url_bar_primary_text_incognito);
        final int defaultColor = MaterialColors.getColor(mContext, R.attr.colorOnSurface, TAG);

        assertEquals(
                "Wrong url bar primary text color for LIGHT_THEME.",
                darkTextColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong url bar primary text color for DARK_THEME.",
                lightTextColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong url bar primary text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mContext, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong url bar primary text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getUrlBarSecondaryTextColor() {
        final int darkTextColor = mContext.getColor(R.color.branded_url_text_variant_on_light_bg);
        final int lightTextColor = mContext.getColor(R.color.branded_url_text_variant_on_dark_bg);
        final int incognitoColor = mContext.getColor(R.color.url_bar_secondary_text_incognito);
        final int defaultColor =
                MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, TAG);

        assertEquals(
                "Wrong url bar secondary text color for LIGHT_THEME.",
                darkTextColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong url bar secondary text color for DARK_THEME.",
                lightTextColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong url bar secondary text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mContext, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong url bar secondary text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getUrlBarDangerColor() {
        final int redOnDark = mContext.getColor(R.color.default_red_light);
        final int redOnLight = mContext.getColor(R.color.default_red_dark);

        assertEquals(
                "Danger color for DARK_THEME should be the lighter red.",
                redOnDark,
                OmniboxResourceProvider.getUrlBarDangerColor(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Danger color for LIGHT_THEME should be the darker red.",
                redOnLight,
                OmniboxResourceProvider.getUrlBarDangerColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Danger color for DEFAULT should be the darker red when we're in light theme.",
                redOnLight,
                OmniboxResourceProvider.getUrlBarDangerColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
        assertEquals(
                "Danger color for INCOGNITO should be the lighter red.",
                redOnDark,
                OmniboxResourceProvider.getUrlBarDangerColor(
                        mContext, BrandedColorScheme.INCOGNITO));
    }

    @Test
    public void getUrlBarSecureColor() {
        final int greenOnDark = mContext.getColor(R.color.default_green_light);
        final int greenOnLight = mContext.getColor(R.color.default_green_dark);

        assertEquals(
                "Secure color for DARK_THEME should be the lighter green.",
                greenOnDark,
                OmniboxResourceProvider.getUrlBarSecureColor(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Secure color for LIGHT_THEME should be the darker green.",
                greenOnLight,
                OmniboxResourceProvider.getUrlBarSecureColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Secure color for DEFAULT should be the darker green when we're in light theme.",
                greenOnLight,
                OmniboxResourceProvider.getUrlBarSecureColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
        assertEquals(
                "Secure color for INCOGNITO should be the lighter green.",
                greenOnDark,
                OmniboxResourceProvider.getUrlBarSecureColor(
                        mContext, BrandedColorScheme.INCOGNITO));
    }

    @Test
    public void getSuggestionPrimaryTextColor() {
        final int incognitoColor = mContext.getColor(R.color.default_text_color_light);
        final int defaultColor = MaterialColors.getColor(mContext, R.attr.colorOnSurface, TAG);

        assertEquals(
                "Wrong suggestion primary text color for LIGHT_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion primary text color for DARK_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion primary text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mContext, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong suggestion primary text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getSuggestionSecondaryTextColor() {
        final int incognitoColor = mContext.getColor(R.color.default_text_color_secondary_light);
        final int defaultColor =
                MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, TAG);

        assertEquals(
                "Wrong suggestion secondary text color for LIGHT_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion secondary text color for DARK_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion secondary text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mContext, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong suggestion secondary text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getSuggestionUrlTextColor() {
        final int incognitoColor = mContext.getColor(R.color.suggestion_url_color_incognito);
        final int defaultColor = SemanticColorUtils.getDefaultTextColorLink(mContext);

        assertEquals(
                "Wrong suggestion url text color for LIGHT_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion url text color for DARK_THEME.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong suggestion url text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mContext, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong suggestion url text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getStatusSeparatorColor() {
        final int darkColor = mContext.getColor(R.color.locationbar_status_separator_color_dark);
        final int lightColor = mContext.getColor(R.color.locationbar_status_separator_color_light);
        final int incognitoColor =
                mContext.getColor(R.color.locationbar_status_separator_color_incognito);
        final int defaultColor = MaterialColors.getColor(mContext, R.attr.colorOutline, TAG);

        assertEquals(
                "Wrong status separator color for LIGHT_THEME.",
                darkColor,
                OmniboxResourceProvider.getStatusSeparatorColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong status separator color for DARK_THEME.",
                lightColor,
                OmniboxResourceProvider.getStatusSeparatorColor(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong status separator color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getStatusSeparatorColor(
                        mContext, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong status separator color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getStatusSeparatorColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getStatusPreviewTextColor() {
        final int darkColor = mContext.getColor(R.color.locationbar_status_preview_color_dark);
        final int lightColor = mContext.getColor(R.color.locationbar_status_preview_color_light);
        final int incognitoColor =
                mContext.getColor(R.color.locationbar_status_preview_color_incognito);
        final int defaultColor = MaterialColors.getColor(mContext, R.attr.colorPrimary, TAG);

        assertEquals(
                "Wrong status preview text color for LIGHT_THEME.",
                darkColor,
                OmniboxResourceProvider.getStatusPreviewTextColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong status preview text color for DARK_THEME.",
                lightColor,
                OmniboxResourceProvider.getStatusPreviewTextColor(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong status preview text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getStatusPreviewTextColor(
                        mContext, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong status preview text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getStatusPreviewTextColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void getStatusOfflineTextColor() {
        final int darkColor = mContext.getColor(R.color.locationbar_status_offline_color_dark);
        final int lightColor = mContext.getColor(R.color.locationbar_status_offline_color_light);
        final int incognitoColor =
                mContext.getColor(R.color.locationbar_status_offline_color_incognito);
        final int defaultColor =
                MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, TAG);

        assertEquals(
                "Wrong status offline text color for LIGHT_THEME.",
                darkColor,
                OmniboxResourceProvider.getStatusOfflineTextColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME));
        assertEquals(
                "Wrong status offline text color for DARK_THEME.",
                lightColor,
                OmniboxResourceProvider.getStatusOfflineTextColor(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME));
        assertEquals(
                "Wrong status offline text color for INCOGNITO.",
                incognitoColor,
                OmniboxResourceProvider.getStatusOfflineTextColor(
                        mContext, BrandedColorScheme.INCOGNITO));
        assertEquals(
                "Wrong status offline text color for DEFAULT.",
                defaultColor,
                OmniboxResourceProvider.getStatusOfflineTextColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_CACHE_SUGGESTION_RESOURCES)
    public void getDrawableCached() {
        Drawable drawable =
                OmniboxResourceProvider.getDrawable(mContext, R.drawable.btn_suggestion_refine);
        ConstantState constantState = drawable.getConstantState();

        Assert.assertEquals(
                constantState,
                OmniboxResourceProvider.getDrawableCacheForTesting()
                        .get(R.drawable.btn_suggestion_refine));

        drawable = OmniboxResourceProvider.getDrawable(mContext, R.drawable.btn_suggestion_refine);
        Assert.assertNotNull(drawable);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_CACHE_SUGGESTION_RESOURCES)
    public void getStringCached() {
        String refineString =
                OmniboxResourceProvider.getString(
                        mContext, R.string.accessibility_omnibox_btn_refine, "foobar");

        Assert.assertEquals(
                mContext.getString(R.string.accessibility_omnibox_btn_refine, "foobar"),
                refineString);
        Assert.assertEquals(
                mContext.getString(R.string.accessibility_omnibox_btn_refine),
                OmniboxResourceProvider.getStringCacheForTesting()
                        .get(R.string.accessibility_omnibox_btn_refine));

        String copyString = OmniboxResourceProvider.getString(mContext, R.string.copy_link);
        Assert.assertEquals(mContext.getString(R.string.copy_link, "foobar"), copyString);
        Assert.assertEquals(
                copyString,
                OmniboxResourceProvider.getStringCacheForTesting().get(R.string.copy_link));
    }

    @Test
    public void getString_convertGritPlaceholders() {
        doReturn("Hello, $1!").when(mContext).getString(1234);
        assertEquals("Hello, world!", OmniboxResourceProvider.getString(mContext, 1234, "world"));

        doReturn("$2, $1 $2").when(mContext).getString(1235);
        assertEquals(
                "Bond, James Bond",
                OmniboxResourceProvider.getString(mContext, 1235, "James", "Bond"));

        doReturn("$1s and $2s").when(mContext).getString(1236);
        assertEquals(
                "Burritos and Chimichangas",
                OmniboxResourceProvider.getString(mContext, 1236, "Burrito", "Chimichanga"));

        doReturn("$s and $d").when(mContext).getString(1237);
        assertEquals(
                "$s and $d", OmniboxResourceProvider.getString(mContext, 1237, "Bad", "Broken"));

        doReturn("$1$1$1$1$1$1 $2!!!").when(mContext).getString(1238);
        assertEquals(
                "NaNaNaNaNaNa BATMAN!!!",
                OmniboxResourceProvider.getString(mContext, 1238, "Na", "BATMAN"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_CACHE_SUGGESTION_RESOURCES)
    public void invalidateDrawableCache() {
        Drawable drawable =
                OmniboxResourceProvider.getDrawable(mContext, R.drawable.btn_suggestion_refine);
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
        Context originalContext = mContext;
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
                MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, TAG);
        assertEquals(
                "Wrong additional text color.",
                defaultTextColorSecondary,
                OmniboxResourceProvider.getAdditionalTextColor(mContext));
    }
}
