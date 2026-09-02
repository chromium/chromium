// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Drawable.ConstantState;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;

import com.google.android.material.color.MaterialColors;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Tests for {@link OmniboxResourceProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxResourceProviderUnitTest {
    private static final String TAG = "ORPTest";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @ColorInt int mDefaultColor;
    private Context mContext;
    private OmniboxResourceProvider mProvider;

    @Before
    public void setUp() {
        mContext =
                spy(
                        new ContextThemeWrapper(
                                ContextUtils.getApplicationContext(),
                                R.style.Theme_BrowserUI_DayNight));
        mDefaultColor = ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false);
        mProvider = new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
    }

    @Test
    public void resolveAttributeToDrawable() {
        Drawable drawableLight =
                OmniboxResourceProvider.resolveAttributeToDrawable(
                        mContext,
                        BrandedColorScheme.LIGHT_BRANDED_THEME,
                        R.attr.selectableItemBackground);
        assertNotNull(drawableLight);

        Drawable drawableDark =
                OmniboxResourceProvider.resolveAttributeToDrawable(
                        mContext,
                        BrandedColorScheme.DARK_BRANDED_THEME,
                        R.attr.selectableItemBackground);
        assertNotNull(drawableDark);
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

        mProvider.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(
                "Wrong status separator color for LIGHT_THEME.",
                darkColor,
                mProvider.getStatusSeparatorColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(
                "Wrong status separator color for DARK_THEME.",
                lightColor,
                mProvider.getStatusSeparatorColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(
                "Wrong status separator color for INCOGNITO.",
                incognitoColor,
                mProvider.getStatusSeparatorColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                "Wrong status separator color for DEFAULT.",
                defaultColor,
                mProvider.getStatusSeparatorColor());
    }

    @Test
    public void getStatusPreviewTextColor() {
        final int darkColor = mContext.getColor(R.color.locationbar_status_preview_color_dark);
        final int lightColor = mContext.getColor(R.color.locationbar_status_preview_color_light);
        final int incognitoColor =
                mContext.getColor(R.color.locationbar_status_preview_color_incognito);
        final int defaultColor = MaterialColors.getColor(mContext, R.attr.colorPrimary, TAG);

        mProvider.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(
                "Wrong status preview text color for LIGHT_THEME.",
                darkColor,
                mProvider.getStatusPreviewTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(
                "Wrong status preview text color for DARK_THEME.",
                lightColor,
                mProvider.getStatusPreviewTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(
                "Wrong status preview text color for INCOGNITO.",
                incognitoColor,
                mProvider.getStatusPreviewTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                "Wrong status preview text color for DEFAULT.",
                defaultColor,
                mProvider.getStatusPreviewTextColor());
    }

    @Test
    public void getStatusOfflineTextColor() {
        final int darkColor = mContext.getColor(R.color.locationbar_status_offline_color_dark);
        final int lightColor = mContext.getColor(R.color.locationbar_status_offline_color_light);
        final int incognitoColor =
                mContext.getColor(R.color.locationbar_status_offline_color_incognito);
        final int defaultColor =
                MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, TAG);

        mProvider.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(
                "Wrong status offline text color for LIGHT_THEME.",
                darkColor,
                mProvider.getStatusOfflineTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(
                "Wrong status offline text color for DARK_THEME.",
                lightColor,
                mProvider.getStatusOfflineTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(
                "Wrong status offline text color for INCOGNITO.",
                incognitoColor,
                mProvider.getStatusOfflineTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                "Wrong status offline text color for DEFAULT.",
                defaultColor,
                mProvider.getStatusOfflineTextColor());
    }

    @Test
    public void getSecondaryIconTintList() {
        ColorStateList adaptiveSecondary =
                mContext.getColorStateList(R.color.default_icon_color_secondary_tint_list);
        ColorStateList incognitoSecondary =
                mContext.getColorStateList(R.color.default_icon_color_secondary_light_tint_list);
        OmniboxResourceProvider provider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
        assertEquals(adaptiveSecondary, provider.getSecondaryIconTintList());
        provider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(incognitoSecondary, provider.getSecondaryIconTintList());
    }

    @Test
    public void getFuseboxPopupIconTintList() {
        ColorStateList primaryTint =
                OmniboxResourceProvider.getPrimaryIconTintList(
                        mContext, BrandedColorScheme.APP_DEFAULT);
        ColorStateList secondaryTint =
                OmniboxResourceProvider.getSecondaryIconTintList(
                        mContext, BrandedColorScheme.APP_DEFAULT);
        OmniboxResourceProvider provider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);

        assertEquals(primaryTint, provider.getFuseboxPopupIconTintList(/* isBottomSheet= */ true));
        assertEquals(
                secondaryTint, provider.getFuseboxPopupIconTintList(/* isBottomSheet= */ false));
    }

    @Test
    public void getFuseboxPopupIconBackgroundTintList() {
        ColorStateList primaryBgTint =
                OmniboxResourceProvider.getPrimaryIconBackgroundTintList(
                        mContext, BrandedColorScheme.APP_DEFAULT);
        OmniboxResourceProvider provider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                primaryBgTint,
                provider.getFuseboxPopupIconBackgroundTintList(/* isBottomSheet= */ true));
        assertNull(provider.getFuseboxPopupIconBackgroundTintList(/* isBottomSheet= */ false));
    }

    @Test
    public void getFuseboxPopupIconSize() {
        Resources res = mContext.getResources();
        int expectedBottomSheetSize =
                res.getDimensionPixelSize(R.dimen.fusebox_bottom_sheet_attachment_icon_size);
        int expectedPlusMenuSize = res.getDimensionPixelSize(R.dimen.fusebox_popup_item_icon_size);

        assertEquals(
                expectedBottomSheetSize,
                OmniboxResourceProvider.getFuseboxPopupIconSize(
                        mContext, /* isBottomSheet= */ true));
        assertEquals(
                expectedPlusMenuSize,
                OmniboxResourceProvider.getFuseboxPopupIconSize(
                        mContext, /* isBottomSheet= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_CACHE_SUGGESTION_RESOURCES)
    public void getDrawableCached() {
        Drawable drawable1 = mProvider.getDrawable(R.drawable.btn_suggestion_refine_up);
        assertNotNull(drawable1);
        ConstantState constantState = drawable1.getConstantState();
        assertNotNull(constantState);

        assertEquals(
                constantState,
                mProvider.getDrawableCacheForTesting().get(R.drawable.btn_suggestion_refine_up));

        Drawable drawable2 = mProvider.getDrawable(R.drawable.btn_suggestion_refine_up);
        assertNotNull(drawable2);
        assertEquals(1, mProvider.getDrawableCacheForTesting().size());

        Drawable popupBackground = mProvider.getPopupBackgroundDrawable();
        assertNotNull(popupBackground);

        Drawable popoverPlusBackground = mProvider.getPopoverPlusButtonBackground();
        assertNotNull(popoverPlusBackground);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_CACHE_SUGGESTION_RESOURCES)
    public void getStringCached() {
        String refineString =
                OmniboxResourceProvider.getString(
                        mContext, R.string.accessibility_omnibox_btn_refine, "foobar");

        assertEquals(
                mContext.getString(R.string.accessibility_omnibox_btn_refine, "foobar"),
                refineString);
        assertEquals(
                mContext.getString(R.string.accessibility_omnibox_btn_refine),
                OmniboxResourceProvider.getStringCacheForTesting()
                        .get(R.string.accessibility_omnibox_btn_refine));

        String copyString = OmniboxResourceProvider.getString(mContext, R.string.copy_link);
        assertEquals(mContext.getString(R.string.copy_link, "foobar"), copyString);
        assertEquals(
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
    public void getAdditionalTextColor() {
        final int defaultTextColorSecondary =
                MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, TAG);
        assertEquals(
                "Wrong additional text color.",
                defaultTextColorSecondary,
                OmniboxResourceProvider.getAdditionalTextColor(mContext));
    }

    @Test
    public void testInstanceMethods() {
        OmniboxResourceProvider provider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.LIGHT_BRANDED_THEME);

        // Test getDrawable.
        Drawable drawable = provider.getDrawable(R.drawable.btn_suggestion_refine_up);
        assertNotNull(drawable);

        // Test getString.
        String string = provider.getString(R.string.copy_link);
        assertEquals(mContext.getString(R.string.copy_link), string);

        // Test getDimen.
        int dimen = provider.getDimen(R.dimen.omnibox_suggestion_24dp_icon_size);
        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_24dp_icon_size),
                dimen);

        // Test getColor.
        int color = provider.getColor(R.color.default_red_light);
        assertEquals(mContext.getColor(R.color.default_red_light), color);

        // Test getString with args.
        doReturn("Hello, $1!").when(mContext).getString(9999);
        assertEquals("Hello, world!", provider.getString(9999, "world"));

        // Test color resolution.
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME),
                provider.getUrlBarPrimaryTextColor());

        assertEquals(
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME),
                provider.getSuggestionPrimaryTextColor());

        // Test dynamic update (setter).
        provider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mContext, BrandedColorScheme.INCOGNITO),
                provider.getUrlBarPrimaryTextColor());

        // Test constructor that resolves scheme.
        OmniboxResourceProvider provider2 =
                new OmniboxResourceProvider(mContext, /* isIncognitoBranded= */ false, Color.WHITE);
        @BrandedColorScheme
        int expectedScheme =
                OmniboxResourceProvider.getBrandedColorScheme(mContext, false, Color.WHITE);
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(mContext, expectedScheme),
                provider2.getUrlBarPrimaryTextColor());
    }

    @Test
    public void getPopoverSuggestionBackgroundColor_incognito() {
        final int incognitoColor = mContext.getColor(R.color.gm3_baseline_surface_container_dark);
        assertEquals(
                incognitoColor,
                OmniboxResourceProvider.getPopoverSuggestionBackgroundColor(
                        mContext, BrandedColorScheme.INCOGNITO));
    }

    @Test
    public void getPopoverSuggestionBackgroundColor_light() {
        assertEquals(
                MaterialColors.getColor(mContext, R.attr.colorSurface, TAG),
                OmniboxResourceProvider.getPopoverSuggestionBackgroundColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    @Config(qualifiers = "night")
    public void getPopoverSuggestionBackgroundColor_dark() {
        assertEquals(
                SemanticColorUtils.getColorSurfaceContainer(mContext),
                OmniboxResourceProvider.getPopoverSuggestionBackgroundColor(
                        mContext, BrandedColorScheme.APP_DEFAULT));
    }

    @Test
    public void testInstanceCachingAndConfigurationChange() {
        OmniboxResourceProvider provider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);

        ResourceCache cache1 = provider.getCacheForTesting();
        assertNotNull(cache1);

        // 1. Verify changing branded color scheme updates scheme but does NOT invalidate cache.
        provider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(BrandedColorScheme.INCOGNITO, provider.getBrandedColorScheme());
        assertSame(cache1, provider.getCacheForTesting());

        // 2. Verify onConfigurationChanged invalidates cache when system night mode changes.
        Configuration config = new Configuration(mContext.getResources().getConfiguration());
        config.uiMode = Configuration.UI_MODE_NIGHT_YES;
        provider.onConfigurationChanged(config);
        ResourceCache cache2 = provider.getCacheForTesting();
        assertNotSame(cache1, cache2);
    }
}
