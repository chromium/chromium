// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;

import androidx.annotation.DrawableRes;
import androidx.core.content.ContextCompat;

import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.omnibox.R;

/** Provides resources specific to Omnibox. */
public class OmniboxResourceProvider {
    /**
     * Provides {@link OmniboxTheme} appropriate for the current UI, specifically whether we are
     * showing incognito and if not which theme is selected.
     *
     * @param useDarkColors whether UI Theme is dark or not.
     * @param isIncognito Whether the Omnibox is shown in incognito at the moment.
     * @return {@link OmniboxTheme} matching the provided parameters.
     */
    public static @OmniboxTheme int getThemeFromDarkColorsAndIncognito(
            boolean useDarkColors, boolean isIncognito) {
        if (isIncognito) return OmniboxTheme.INCOGNITO;
        if (useDarkColors) return OmniboxTheme.LIGHT_THEME;
        return OmniboxTheme.DARK_THEME;
    }

    /** @return Whether the mode is dark (dark theme or incognito). */
    public static boolean isDarkMode(@OmniboxTheme int omniboxTheme) {
        return omniboxTheme == OmniboxTheme.DARK_THEME || omniboxTheme == OmniboxTheme.INCOGNITO;
    }

    /**
     * Returns a drawable for a given attribute depending on a {@link OmniboxTheme}
     *
     * @param context The {@link Context} used to retrieve resources.
     * @param omniboxTheme {@link OmniboxTheme} to use.
     * @param attributeResId A resource ID of an attribute to resolve.
     * @return A background drawable resource ID providing ripple effect.
     */
    public static Drawable resolveAttributeToDrawable(
            Context context, @OmniboxTheme int omniboxTheme, int attributeResId) {
        Context wrappedContext = maybeWrapContext(context, omniboxTheme);
        @DrawableRes
        int resourceId = resolveAttributeToDrawableRes(wrappedContext, attributeResId);
        return ContextCompat.getDrawable(wrappedContext, resourceId);
    }

    /**
     * Wraps the context if necessary to force dark resources for incognito.
     *
     * @param context The {@link Context} to be wrapped.
     * @param omniboxTheme Current omnibox theme.
     * @return Context with resources appropriate to the {@link OmniboxTheme}.
     */
    private static Context maybeWrapContext(Context context, @OmniboxTheme int omniboxTheme) {
        // Only wraps the context in case of incognito.
        if (omniboxTheme == OmniboxTheme.INCOGNITO) {
            return NightModeUtils.wrapContextWithNightModeConfig(
                    context, R.style.Theme_Chromium_TabbedMode, /*nightMode=*/true);
        }

        return context;
    }

    /**
     * Resolves the attribute based on the current theme.
     *
     * @param context The {@link Context} used to retrieve resources.
     * @param attributeResId Resource ID of the attribute to resolve.
     * @return Resource ID of the expected drawable.
     */
    private static @DrawableRes int resolveAttributeToDrawableRes(
            Context context, int attributeResId) {
        TypedValue themeRes = new TypedValue();
        context.getTheme().resolveAttribute(attributeResId, themeRes, true);
        return themeRes.resourceId;
    }
}
