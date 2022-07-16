// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;

import com.google.android.material.color.MaterialColors;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link OmniboxResourceProvider}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxResourceProviderTest {
    private static final String TAG = "ORPTest";

    private Activity mActivity;

    private @ColorInt int mDefaultColor;
    private @ColorInt int mDefaultIncognitoColor;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mDefaultColor = ChromeColors.getDefaultThemeColor(mActivity, false);
        mDefaultIncognitoColor = ChromeColors.getDefaultThemeColor(mActivity, true);
    }

    @Test
    public void isDarkMode() {
        Assert.assertTrue(OmniboxResourceProvider.isDarkMode(OmniboxTheme.DARK_THEME));
        Assert.assertTrue(OmniboxResourceProvider.isDarkMode(OmniboxTheme.INCOGNITO));
        Assert.assertFalse(OmniboxResourceProvider.isDarkMode(OmniboxTheme.LIGHT_THEME));
    }

    @Test
    public void resolveAttributeToDrawable() {
        Drawable drawableLight = OmniboxResourceProvider.resolveAttributeToDrawable(
                mActivity, OmniboxTheme.LIGHT_THEME, R.attr.selectableItemBackground);
        Assert.assertNotNull(drawableLight);

        Drawable drawableDark = OmniboxResourceProvider.resolveAttributeToDrawable(
                mActivity, OmniboxTheme.DARK_THEME, R.attr.selectableItemBackground);
        Assert.assertNotNull(drawableDark);
    }

    @Test
    public void getOmniboxTheme_incognito() {
        assertEquals("Omnibox theme should be INCOGNITO.", OmniboxTheme.INCOGNITO,
                OmniboxResourceProvider.getOmniboxTheme(mActivity, true, mDefaultColor));
        assertEquals("Omnibox theme should be INCOGNITO.", OmniboxTheme.INCOGNITO,
                OmniboxResourceProvider.getOmniboxTheme(mActivity, true, Color.RED));
    }

    @Test
    public void getOmniboxTheme_nonIncognito() {
        assertEquals("Omnibox theme should be DEFAULT.", OmniboxTheme.DEFAULT,
                OmniboxResourceProvider.getOmniboxTheme(mActivity, false, mDefaultColor));
        assertEquals("Omnibox theme should be DARK_THEME.", OmniboxTheme.DARK_THEME,
                OmniboxResourceProvider.getOmniboxTheme(mActivity, false, Color.BLACK));
        assertEquals("Omnibox theme should be LIGHT_THEME.", OmniboxTheme.LIGHT_THEME,
                OmniboxResourceProvider.getOmniboxTheme(
                        mActivity, false, Color.parseColor("#eaecf0" /*Light grey color*/)));
    }

    @Test
    public void getUrlBarPrimaryTextColor() {
        final Resources resources = mActivity.getResources();
        final int darkTextColor = resources.getColor(R.color.branded_url_text_on_light_bg);
        final int lightTextColor = resources.getColor(R.color.branded_url_text_on_dark_bg);
        final int incognitoColor = resources.getColor(R.color.url_bar_primary_text_incognito);
        final int defaultColor = MaterialColors.getColor(mActivity, R.attr.colorOnSurface, TAG);

        assertEquals("Wrong url bar primary text color for LIGHT_THEME.", darkTextColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, OmniboxTheme.LIGHT_THEME));
        assertEquals("Wrong url bar primary text color for DARK_THEME.", lightTextColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, OmniboxTheme.DARK_THEME));
        assertEquals("Wrong url bar primary text color for INCOGNITO.", incognitoColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, OmniboxTheme.INCOGNITO));
        assertEquals("Wrong url bar primary text color for DEFAULT.", defaultColor,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(mActivity, OmniboxTheme.DEFAULT));
    }

    @Test
    public void getUrlBarSecondaryTextColor() {
        final Resources resources = mActivity.getResources();
        final int darkTextColor = resources.getColor(R.color.branded_url_text_variant_on_light_bg);
        final int lightTextColor = resources.getColor(R.color.branded_url_text_variant_on_dark_bg);
        final int incognitoColor = resources.getColor(R.color.url_bar_secondary_text_incognito);
        final int defaultColor =
                MaterialColors.getColor(mActivity, R.attr.colorOnSurfaceVariant, TAG);

        assertEquals("Wrong url bar secondary text color for LIGHT_THEME.", darkTextColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mActivity, OmniboxTheme.LIGHT_THEME));
        assertEquals("Wrong url bar secondary text color for DARK_THEME.", lightTextColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mActivity, OmniboxTheme.DARK_THEME));
        assertEquals("Wrong url bar secondary text color for INCOGNITO.", incognitoColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mActivity, OmniboxTheme.INCOGNITO));
        assertEquals("Wrong url bar secondary text color for DEFAULT.", defaultColor,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                        mActivity, OmniboxTheme.DEFAULT));
    }

    @Test
    public void getUrlBarDangerColor() {
        final Resources resources = mActivity.getResources();
        final int redOnDark = resources.getColor(R.color.default_red_light);
        final int redOnLight = resources.getColor(R.color.default_red_dark);

        assertEquals("Danger color for DARK_THEME should be the lighter red.", redOnDark,
                OmniboxResourceProvider.getUrlBarDangerColor(mActivity, OmniboxTheme.DARK_THEME));
        assertEquals("Danger color for LIGHT_THEME should be the darker red.", redOnLight,
                OmniboxResourceProvider.getUrlBarDangerColor(mActivity, OmniboxTheme.LIGHT_THEME));
        assertEquals("Danger color for DEFAULT should be the darker red when we're in light theme.",
                redOnLight,
                OmniboxResourceProvider.getUrlBarDangerColor(mActivity, OmniboxTheme.DEFAULT));
        assertEquals("Danger color for INCOGNITO should be the lighter red.", redOnDark,
                OmniboxResourceProvider.getUrlBarDangerColor(mActivity, OmniboxTheme.INCOGNITO));
    }

    @Test
    public void getUrlBarSecureColor() {
        final Resources resources = mActivity.getResources();
        final int greenOnDark = resources.getColor(R.color.default_green_light);
        final int greenOnLight = resources.getColor(R.color.default_green_dark);

        assertEquals("Secure color for DARK_THEME should be the lighter green.", greenOnDark,
                OmniboxResourceProvider.getUrlBarSecureColor(mActivity, OmniboxTheme.DARK_THEME));
        assertEquals("Secure color for LIGHT_THEME should be the darker green.", greenOnLight,
                OmniboxResourceProvider.getUrlBarSecureColor(mActivity, OmniboxTheme.LIGHT_THEME));
        assertEquals(
                "Secure color for DEFAULT should be the darker green when we're in light theme.",
                greenOnLight,
                OmniboxResourceProvider.getUrlBarSecureColor(mActivity, OmniboxTheme.DEFAULT));
        assertEquals("Secure color for INCOGNITO should be the lighter green.", greenOnDark,
                OmniboxResourceProvider.getUrlBarSecureColor(mActivity, OmniboxTheme.INCOGNITO));
    }

    @Test
    public void getSuggestionPrimaryTextColor() {
        final Resources resources = mActivity.getResources();
        final int incognitoColor = resources.getColor(R.color.default_text_color_light);
        final int defaultColor = MaterialColors.getColor(mActivity, R.attr.colorOnSurface, TAG);

        assertEquals("Wrong suggestion primary text color for LIGHT_THEME.", defaultColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mActivity, OmniboxTheme.LIGHT_THEME));
        assertEquals("Wrong suggestion primary text color for DARK_THEME.", defaultColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mActivity, OmniboxTheme.DARK_THEME));
        assertEquals("Wrong suggestion primary text color for INCOGNITO.", incognitoColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mActivity, OmniboxTheme.INCOGNITO));
        assertEquals("Wrong suggestion primary text color for DEFAULT.", defaultColor,
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                        mActivity, OmniboxTheme.DEFAULT));
    }

    @Test
    public void getSuggestionSecondaryTextColor() {
        final Resources resources = mActivity.getResources();
        final int incognitoColor = resources.getColor(R.color.default_text_color_secondary_light);
        final int defaultColor =
                MaterialColors.getColor(mActivity, R.attr.colorOnSurfaceVariant, TAG);

        assertEquals("Wrong suggestion secondary text color for LIGHT_THEME.", defaultColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mActivity, OmniboxTheme.LIGHT_THEME));
        assertEquals("Wrong suggestion secondary text color for DARK_THEME.", defaultColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mActivity, OmniboxTheme.DARK_THEME));
        assertEquals("Wrong suggestion secondary text color for INCOGNITO.", incognitoColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mActivity, OmniboxTheme.INCOGNITO));
        assertEquals("Wrong suggestion secondary text color for DEFAULT.", defaultColor,
                OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                        mActivity, OmniboxTheme.DEFAULT));
    }

    @Test
    public void getSuggestionUrlTextColor() {
        final Resources resources = mActivity.getResources();
        final int incognitoColor = resources.getColor(R.color.suggestion_url_color_incognito);
        final int defaultColor = resources.getColor(R.color.suggestion_url_color);

        assertEquals("Wrong suggestion url text color for LIGHT_THEME.", defaultColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mActivity, OmniboxTheme.LIGHT_THEME));
        assertEquals("Wrong suggestion url text color for DARK_THEME.", defaultColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mActivity, OmniboxTheme.DARK_THEME));
        assertEquals("Wrong suggestion url text color for INCOGNITO.", incognitoColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(
                        mActivity, OmniboxTheme.INCOGNITO));
        assertEquals("Wrong suggestion url text color for DEFAULT.", defaultColor,
                OmniboxResourceProvider.getSuggestionUrlTextColor(mActivity, OmniboxTheme.DEFAULT));
    }
}
