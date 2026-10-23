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
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.Fusebox.FuseboxLayoutMode;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.IncognitoColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatureList;

/** Tests for {@link OmniboxResourceProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxResourceProviderUnitTest {
    private static final String TAG = "ORPTest";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

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
        mProvider.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        Drawable drawableLight =
                mProvider.resolveAttributeToDrawable(R.attr.selectableItemBackground);
        assertNotNull(drawableLight);

        mProvider.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        Drawable drawableDark =
                mProvider.resolveAttributeToDrawable(R.attr.selectableItemBackground);
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

        mProvider.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(
                "Wrong suggestion primary text color for LIGHT_THEME.",
                defaultColor,
                mProvider.getSuggestionPrimaryTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(
                "Wrong suggestion primary text color for DARK_THEME.",
                defaultColor,
                mProvider.getSuggestionPrimaryTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(
                "Wrong suggestion primary text color for INCOGNITO.",
                incognitoColor,
                mProvider.getSuggestionPrimaryTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                "Wrong suggestion primary text color for DEFAULT.",
                defaultColor,
                mProvider.getSuggestionPrimaryTextColor());
    }

    @Test
    public void getSuggestionSecondaryTextColor() {
        final int incognitoColor = mContext.getColor(R.color.default_text_color_secondary_light);
        final int defaultColor =
                MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, TAG);

        mProvider.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(
                "Wrong suggestion secondary text color for LIGHT_THEME.",
                defaultColor,
                mProvider.getSuggestionSecondaryTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(
                "Wrong suggestion secondary text color for DARK_THEME.",
                defaultColor,
                mProvider.getSuggestionSecondaryTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(
                "Wrong suggestion secondary text color for INCOGNITO.",
                incognitoColor,
                mProvider.getSuggestionSecondaryTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                "Wrong suggestion secondary text color for DEFAULT.",
                defaultColor,
                mProvider.getSuggestionSecondaryTextColor());
    }

    @Test
    public void getSuggestionUrlTextColor() {
        final int incognitoColor = mContext.getColor(R.color.suggestion_url_color_incognito);
        final int defaultColor = SemanticColorUtils.getDefaultTextColorLink(mContext);

        mProvider.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(
                "Wrong suggestion url text color for LIGHT_THEME.",
                defaultColor,
                mProvider.getSuggestionUrlTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(
                "Wrong suggestion url text color for DARK_THEME.",
                defaultColor,
                mProvider.getSuggestionUrlTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(
                "Wrong suggestion url text color for INCOGNITO.",
                incognitoColor,
                mProvider.getSuggestionUrlTextColor());
        mProvider.setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                "Wrong suggestion url text color for DEFAULT.",
                defaultColor,
                mProvider.getSuggestionUrlTextColor());
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
        assertEquals(adaptiveSecondary, mProvider.getSecondaryIconTintList());
        assertSame(mProvider.getSecondaryIconTintList(), mProvider.getSecondaryIconTintList());
        assertSame(mProvider.getPrimaryIconTintList(), mProvider.getPrimaryIconTintList());
        mProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(incognitoSecondary, mProvider.getSecondaryIconTintList());
        assertSame(mProvider.getSecondaryIconTintList(), mProvider.getSecondaryIconTintList());
    }

    @Test
    public void getFuseboxPopupIconTintList() {
        ColorStateList primaryTint =
                ChromeColors.getPrimaryIconTint(mContext, /* isIncognito= */ false);
        ColorStateList secondaryTint =
                ChromeColors.getSecondaryIconTint(mContext, /* forceLightIconTint= */ false);
        OmniboxResourceProvider provider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);

        assertEquals(primaryTint, provider.getFuseboxPopupIconTintList(/* isBottomSheet= */ true));
        assertEquals(
                secondaryTint, provider.getFuseboxPopupIconTintList(/* isBottomSheet= */ false));
    }

    @Test
    public void getFuseboxPopupIconBackgroundTintList() {
        ColorStateList primaryBgTint =
                IncognitoColors.getColorSurfaceContainerTintList(
                        mContext, /* isIncognito= */ false);
        OmniboxResourceProvider provider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                primaryBgTint,
                provider.getFuseboxPopupIconBackgroundTintList(/* isBottomSheet= */ true));
        assertNull(provider.getFuseboxPopupIconBackgroundTintList(/* isBottomSheet= */ false));
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getFuseboxPopupIconSize() {
        Resources res = mContext.getResources();
        int expectedBottomSheetSize =
                res.getDimensionPixelSize(R.dimen.fusebox_bottom_sheet_attachment_icon_size);
        int expectedPlusMenuSize = res.getDimensionPixelSize(R.dimen.fusebox_popup_item_icon_size);

        assertEquals(
                expectedBottomSheetSize,
                mProvider.getFuseboxPopupIconSize(/* isBottomSheet= */ true));
        assertEquals(
                expectedPlusMenuSize,
                mProvider.getFuseboxPopupIconSize(/* isBottomSheet= */ false));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getFuseboxPopupIconSize_variationsEnabled() {
        Resources res = mContext.getResources();
        int expectedBottomSheetSize =
                res.getDimensionPixelSize(R.dimen.fusebox_bottom_sheet_attachment_icon_size);
        int expectedPlusMenuSize =
                res.getDimensionPixelSize(R.dimen.fusebox_popup_item_large_icon_size);

        assertEquals(
                expectedBottomSheetSize,
                mProvider.getFuseboxPopupIconSize(/* isBottomSheet= */ true));
        assertEquals(
                expectedPlusMenuSize,
                mProvider.getFuseboxPopupIconSize(/* isBottomSheet= */ false));
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getFuseboxCarouselIconSize() {
        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_bottom_sheet_attachment_icon_size),
                mProvider.getFuseboxCarouselIconSize());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getFuseboxCarouselIconSize_variationsEnabled() {
        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_carousel_item_large_icon_size),
                mProvider.getFuseboxCarouselIconSize());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getPopupItemTextRes_variationsDisabled() {
        int expected = IncognitoColors.getTextMediumPrimary(/* isIncognito= */ false);

        assertEquals(expected, mProvider.getPopupButtonTextRes());
        assertEquals(expected, mProvider.getAttachmentButtonTextRes());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getPopupItemTextRes_variationsEnabled() {
        assertEquals(
                IncognitoColors.getTextLargePrimary(/* isIncognito= */ false),
                mProvider.getPopupButtonTextRes());
        assertEquals(
                IncognitoColors.getTextMediumPrimary(/* isIncognito= */ false),
                mProvider.getAttachmentButtonTextRes());

        mProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(
                IncognitoColors.getTextLargePrimary(/* isIncognito= */ true),
                mProvider.getPopupButtonTextRes());
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
    public void getString_convertGritPlaceholders() {
        doReturn("Hello, $1!").when(mContext).getString(1234);
        assertEquals("Hello, world!", mProvider.getString(1234, "world"));

        doReturn("$2, $1 $2").when(mContext).getString(1235);
        assertEquals("Bond, James Bond", mProvider.getString(1235, "James", "Bond"));

        doReturn("$1s and $2s").when(mContext).getString(1236);
        assertEquals(
                "Burritos and Chimichangas", mProvider.getString(1236, "Burrito", "Chimichanga"));

        doReturn("$s and $d").when(mContext).getString(1237);
        assertEquals("$s and $d", mProvider.getString(1237, "Bad", "Broken"));

        doReturn("$1$1$1$1$1$1 $2!!!").when(mContext).getString(1238);
        assertEquals("NaNaNaNaNaNa BATMAN!!!", mProvider.getString(1238, "Na", "BATMAN"));
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
                MaterialColors.getColor(mContext, R.attr.colorOnSurface, TAG),
                provider.getSuggestionPrimaryTextColor());

        // Test dynamic update (setter).
        provider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mContext, BrandedColorScheme.INCOGNITO),
                provider.getUrlBarPrimaryTextColor());
    }

    @Test
    public void getPopoverSuggestionBackgroundColor_incognito() {
        final int incognitoColor = mContext.getColor(R.color.gm3_baseline_surface_container_dark);
        mProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        assertEquals(
                incognitoColor,
                mProvider.getSuggestionBackgroundColor(
                        FuseboxLayoutMode.SUGGESTIONS_POPOVER, /* isDropdownContainer= */ false));
    }

    @Test
    public void getPopoverSuggestionBackgroundColor_light() {
        assertEquals(
                MaterialColors.getColor(mContext, R.attr.colorSurface, TAG),
                mProvider.getSuggestionBackgroundColor(
                        FuseboxLayoutMode.SUGGESTIONS_POPOVER, /* isDropdownContainer= */ false));
    }

    @Test
    @Config(qualifiers = "night")
    public void getPopoverSuggestionBackgroundColor_dark() {
        assertEquals(
                SemanticColorUtils.getColorSurfaceContainer(mContext),
                mProvider.getSuggestionBackgroundColor(
                        FuseboxLayoutMode.SUGGESTIONS_POPOVER, /* isDropdownContainer= */ false));
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

    @Test
    public void testConfigurationChange_densityInvalidatesCache() {
        OmniboxResourceProvider provider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
        ResourceCache cache1 = provider.getCacheForTesting();

        Configuration config = new Configuration(mContext.getResources().getConfiguration());
        provider.onConfigurationChanged(config);
        assertSame(cache1, provider.getCacheForTesting());

        config.densityDpi = config.densityDpi * 2;
        provider.onConfigurationChanged(config);
        assertNotSame(cache1, provider.getCacheForTesting());
    }

    @Test
    public void testConfigurationChange_fontScaleInvalidatesCache() {
        OmniboxResourceProvider provider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
        ResourceCache cache1 = provider.getCacheForTesting();

        Configuration config = new Configuration(mContext.getResources().getConfiguration());
        config.fontScale = config.fontScale * 1.5f;
        provider.onConfigurationChanged(config);
        assertNotSame(cache1, provider.getCacheForTesting());
    }

    @Test
    public void getStatusIconDimensions_default() {
        assertEquals(
                R.dimen.omnibox_search_engine_logo_composed_half_size,
                mProvider.getStatusIconCornerRadiusRes(/* focused= */ false));
        assertEquals(
                R.dimen.omnibox_small_icon_rounding_radius,
                mProvider.getStatusIconCornerRadiusRes(/* focused= */ true));
        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_status_icon_width),
                mProvider.getStatusIconSize());
    }

    @Test
    public void getStatusIconDimensions_desktop() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);

        assertEquals(
                R.dimen.omnibox_search_engine_logo_composed_half_size_desktop,
                mProvider.getStatusIconCornerRadiusRes(/* focused= */ false));
        assertEquals(
                R.dimen.omnibox_small_icon_rounding_radius,
                mProvider.getStatusIconCornerRadiusRes(/* focused= */ true));
        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(
                                R.dimen.omnibox_search_engine_logo_composed_size_desktop),
                mProvider.getStatusIconSize());
    }

    @Test
    public void getPopupAttachmentsHeaderTextRes() {
        assertEquals(
                IncognitoColors.getTextMediumThickSecondary(/* isIncognito= */ false),
                mProvider.getPopupAttachmentsHeaderTextRes(/* useLarge= */ false));
        assertEquals(
                IncognitoColors.getHeadline2ThickPrimary(/* isIncognito= */ false),
                mProvider.getPopupAttachmentsHeaderTextRes(/* useLarge= */ true));

        OmniboxResourceProvider incognitoProvider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.INCOGNITO);
        assertEquals(
                IncognitoColors.getTextMediumThickSecondary(/* isIncognito= */ true),
                incognitoProvider.getPopupAttachmentsHeaderTextRes(/* useLarge= */ false));
        assertEquals(
                IncognitoColors.getHeadline2ThickPrimary(/* isIncognito= */ true),
                incognitoProvider.getPopupAttachmentsHeaderTextRes(/* useLarge= */ true));
    }
}
