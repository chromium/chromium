// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.DrawableWrapper;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.OmniboxCapabilities;

/** Represents graphical decoration for the suggestion components. */
@NullMarked
public class OmniboxDrawableState {

    /** Embedded drawable object. */
    public final Drawable drawable;

    /** Embedded drawable object for incognito mode. */
    public final Drawable incognitoDrawable;

    /** Whether supplied drawable can be tinted */
    public final boolean allowTint;

    /** Whether drawable should be rounded. */
    public final boolean useRoundedCorners;

    /** Whether drawable should be displayed as large. */
    public final boolean isLarge;

    /**
     * Create OmniboxDrawableState representing a Color.
     *
     * @param color the color to apply
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forColor(@ColorInt int color, Context context) {
        return new OmniboxDrawableState(
                new ColorDrawable(color),
                context,
                /* useRoundedCorners= */ true,
                /* isLarge= */ true,
                /* allowTint= */ false);
    }

    /**
     * Create OmniboxDrawableState representing a small fallback icon.
     *
     * @param context current context
     * @param resourceId resource ID of the drawable
     * @param allowTint whether the icon should be tinted with text color
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forSmallIcon(
            Context context, @DrawableRes int resourceId, boolean allowTint) {
        return new OmniboxDrawableState(
                OmniboxResourceProvider.getDrawable(context, resourceId),
                context,
                /* useRoundedCorners= */ false,
                /* isLarge= */ false,
                allowTint);
    }

    /**
     * Create OmniboxDrawableState representing a small fallback icon.
     *
     * @param context current context
     * @param resourceId resource ID of the drawable
     * @param incognitoResourceId resource ID of the drawable in incognito mode
     * @param allowTint whether the icon should be tinted with text color
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forSmallIconWithIncognitoVariant(
            Context context,
            @DrawableRes int resourceId,
            @DrawableRes int incognitoResourceId,
            boolean allowTint) {
        return new OmniboxDrawableState(
                OmniboxResourceProvider.getDrawable(context, resourceId),
                OmniboxResourceProvider.getDrawable(context, incognitoResourceId),
                context,
                /* useRoundedCorners= */ false,
                /* isLarge= */ false,
                allowTint);
    }

    /**
     * Create OmniboxDrawableState representing a large fallback icon.
     *
     * @param context current context
     * @param resourceId resource ID of the drawable
     * @param allowTint whether the icon should be tinted with text color
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forLargeIcon(
            Context context, @DrawableRes int resourceId, boolean allowTint) {
        return new OmniboxDrawableState(
                OmniboxResourceProvider.getDrawable(context, resourceId),
                context,
                /* useRoundedCorners= */ false,
                /* isLarge= */ true,
                allowTint);
    }

    /**
     * Create OmniboxDrawableState representing a site favicon.
     *
     * @param drawable Drawable of the favicon
     * @param context current context
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forFavIcon(Drawable drawable, Context context) {
        return new OmniboxDrawableState(
                drawable,
                context,
                /* useRoundedCorners= */ true,
                /* isLarge= */ false,
                /* allowTint= */ false);
    }

    /**
     * Create OmniboxDrawableState with dedicated image decoration.
     *
     * @param context current context
     * @param drawable dedicated drawable
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forImage(Drawable drawable, Context context) {
        return new OmniboxDrawableState(
                drawable,
                context,
                /* useRoundedCorners= */ true,
                /* isLarge= */ true,
                /* allowTint= */ false);
    }

    /**
     * Create new OmniboxDrawableState.
     *
     * @param drawable the object to draw
     * @param incognitoDrawable the object to draw in incognito mode
     * @param useRoundedCorners whether to round drawable's corners
     * @param isLarge whether the drawable should be shown as large item
     * @param allowTint whether the icon should be tinted with text color
     */
    @VisibleForTesting
    public OmniboxDrawableState(
            Drawable drawable,
            Drawable incognitoDrawable,
            Context context,
            boolean useRoundedCorners,
            boolean isLarge,
            boolean allowTint) {
        this.drawable = resizeForDesktop(drawable, context, isLarge);
        this.incognitoDrawable = resizeForDesktop(incognitoDrawable, context, isLarge);
        this.useRoundedCorners = useRoundedCorners;
        this.isLarge = isLarge;
        this.allowTint = allowTint;
    }

    /**
     * Create new OmniboxDrawableState.
     *
     * @param drawable the object to draw
     * @param useRoundedCorners whether to round drawable's corners
     * @param isLarge whether the drawable should be shown as large item
     * @param allowTint whether the icon should be tinted with text color
     */
    @VisibleForTesting
    public OmniboxDrawableState(
            Drawable drawable,
            Context context,
            boolean useRoundedCorners,
            boolean isLarge,
            boolean allowTint) {
        this(drawable, drawable, context, useRoundedCorners, isLarge, allowTint);
    }

    /** Inspects the drawable and wraps it to override the size if the platform is Desktop. */
    private static Drawable resizeForDesktop(Drawable drawable, Context context, boolean isLarge) {
        if (drawable == null || !OmniboxCapabilities.isDesktopPlatform()) {
            return drawable;
        }

        Resources resources = context.getResources();
        int targetPx =
                isLarge
                        ? resources.getDimensionPixelSize(
                                R.dimen.omnibox_desktop_large_decoration_icon_size)
                        : resources.getDimensionPixelSize(
                                R.dimen.omnibox_desktop_small_decoration_icon_size);
        return new DrawableWrapper(drawable) {
            @Override
            public int getIntrinsicWidth() {
                return targetPx;
            }

            @Override
            public int getIntrinsicHeight() {
                return targetPx;
            }
        };
    }
}
