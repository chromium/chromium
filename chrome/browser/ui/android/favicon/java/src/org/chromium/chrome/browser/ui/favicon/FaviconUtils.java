// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.favicon;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;

import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

/** Utilities to deal with favicons. */
public class FaviconUtils {
    /**
     * Creates a {@link RoundedIconGenerator} to generate circular {@link Bitmap}s of favicons.
     * @param context The {@link Context} for accessing color and dimen resources.
     * @return A {@link RoundedIconGenerator} that uses the default circle icon style. Intended
     *         for monograms, e.g. a circle with character(s) in the center.
     */
    public static RoundedIconGenerator createCircularIconGenerator(Context context) {
        Resources resources = context.getResources();
        int displayedIconSize = resources.getDimensionPixelSize(R.dimen.circular_monogram_size);
        int cornerRadius = displayedIconSize / 2;
        int textSize = resources.getDimensionPixelSize(R.dimen.circular_monogram_text_size);
        return new RoundedIconGenerator(
                displayedIconSize,
                displayedIconSize,
                cornerRadius,
                getIconColor(context),
                textSize);
    }

    /**
     * Creates a {@link RoundedIconGenerator} to generate rounded rectangular {@link Bitmap}s of
     * favicons.
     * @param context The {@link Context} for accessing color and dimen resources.
     * @return A {@link RoundedIconGenerator} that uses the default rounded rectangle icon
     *         style. Intended for monograms, e.g. a rounded rectangle with character(s) in the
     *         center.
     */
    public static RoundedIconGenerator createRoundedRectangleIconGenerator(Context context) {
        Resources resources = context.getResources();
        int displayedIconSize = resources.getDimensionPixelSize(R.dimen.default_favicon_size);
        int cornerRadius = resources.getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
        int textSize = resources.getDimensionPixelSize(R.dimen.default_favicon_icon_text_size);
        return new RoundedIconGenerator(
                displayedIconSize,
                displayedIconSize,
                cornerRadius,
                getIconColor(context),
                textSize);
    }

    /**
     * Creates a {@link RoundedBitmapDrawable} using the provided {@link Bitmap} and a default
     * favicon corner radius.
     * @param resources The {@link Resources}.
     * @param icon The {@link Bitmap} to round.
     * @return A {@link RoundedBitmapDrawable} for the provided {@link Bitmap}.
     */
    public static RoundedBitmapDrawable createRoundedBitmapDrawable(
            Resources resources, Bitmap icon) {
        return ViewUtils.createRoundedBitmapDrawable(
                resources,
                icon,
                resources.getDimensionPixelSize(R.dimen.default_favicon_corner_radius));
    }

    /**
     * Create a bitmap with corresponding size of a generic favicon.
     * @param context {@link Context} to read the generic favicon.
     * @param size Desired size of the bitmap.
     * @param backgroundColor Optional background color for the favicon.
     * @return A generic globe favicon.
     */
    public static Bitmap createGenericFaviconBitmap(
            Context context, int size, @Nullable @ColorInt Integer backgroundColor) {
        Bitmap bitmap =
                Bitmap.createBitmap(
                        context.getResources().getDisplayMetrics(), size, size, Config.ARGB_8888);
        Drawable drawable = AppCompatResources.getDrawable(context, R.drawable.ic_globe_48dp);
        drawable.setBounds(0, 0, size, size);
        Canvas canvas = new Canvas(bitmap);
        if (backgroundColor != null) {
            canvas.drawColor(backgroundColor);
        }
        drawable.draw(canvas);
        return bitmap;
    }

    /**
     * Creates a {@link Drawable} with the provided icon with
     * nearest-neighbor scaling through {@link Bitmap#createScaledBitmap(Bitmap, int, int,
     * boolean)}, or a fallback monogram.
     * @param icon {@link Bitmap} with the icon to display. If null, a fallback monogram will be
     *         generated.
     * @param url Url to generate a monogram. Used only if {@code icon} is null.
     * @param fallbackColor Color to generate a monogram. Used only if {@code icon} is null.
     * @param iconGenerator RoundedIconGenerator to generate a monogram. Used only if {@code
     *         icon} is null. Side effect: {@link RoundedIconGenerator#setBackgroundColor(int)}
     *         will be called.
     * @param resources {@link Resources} to create a {@link BitmapDrawable}.
     * @param iconSize Width and height of the returned icon in px.
     * @return A {@link Drawable} to be displayed as the favicon.
     */
    public static Drawable getIconDrawableWithoutFilter(
            @Nullable Bitmap icon,
            GURL url,
            @ColorInt int fallbackColor,
            RoundedIconGenerator iconGenerator,
            Resources resources,
            int iconSize) {
        return getIconDrawableWithoutFilter(
                icon, url.getSpec(), fallbackColor, iconGenerator, resources, iconSize);
    }

    @Deprecated // Use GURL variant instead.
    public static Drawable getIconDrawableWithoutFilter(
            @Nullable Bitmap icon,
            String url,
            @ColorInt int fallbackColor,
            RoundedIconGenerator iconGenerator,
            Resources resources,
            int iconSize) {
        if (icon == null) {
            iconGenerator.setBackgroundColor(fallbackColor);
            icon = iconGenerator.generateIconForUrl(url);
            return new BitmapDrawable(resources, icon);
        }
        return createRoundedBitmapDrawable(
                resources, Bitmap.createScaledBitmap(icon, iconSize, iconSize, false));
    }

    /**
     * Creates a {@link Drawable} with the provided icon, or a fallback monogram, with bilinear
     * scaling through {@link Bitmap#createScaledBitmap(Bitmap, int, int, boolean)}, or a fallback
     * default favicon.
     * @param icon {@link Bitmap} with the icon to display. If null, a fallback monogram will be
     *         generated.
     * @param url Url to generate a monogram. Used only if {@code icon} is null.
     * @param iconGenerator RoundedIconGenerator to generate a monogram. Used only if {@code
     *         icon} is null.
     * @param defaultFaviconHelper Helper to generate default favicons.
     * @param context {@link Context} to create a {@link BitmapDrawable} and resolve resources.
     * @param iconSize Width and height of the returned icon.
     * @return A {@link Drawable} to be displayed as the favicon.
     */
    public static Drawable getIconDrawableWithFilter(
            @Nullable Bitmap icon,
            @Nullable GURL url,
            RoundedIconGenerator iconGenerator,
            FaviconHelper.DefaultFaviconHelper defaultFaviconHelper,
            Context context,
            int iconSize) {
        if (url == null) {
            return defaultFaviconHelper.getDefaultFaviconDrawable(context, url, true);
        }
        Resources resources = context.getResources();
        if (icon == null) {
            icon = iconGenerator.generateIconForUrl(url);
            return new BitmapDrawable(
                    resources, Bitmap.createScaledBitmap(icon, iconSize, iconSize, true));
        }
        return createRoundedBitmapDrawable(
                resources, Bitmap.createScaledBitmap(icon, iconSize, iconSize, true));
    }

    private static int getIconColor(Context context) {
        return context.getColor(R.color.default_favicon_background_color);
    }
}
