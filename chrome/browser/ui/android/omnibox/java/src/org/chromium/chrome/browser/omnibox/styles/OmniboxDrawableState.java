// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;

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
    public static OmniboxDrawableState forColor(@ColorInt int color) {
        return new OmniboxDrawableState(
                new ColorDrawable(color),
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
                /* useRoundedCorners= */ false,
                /* isLarge= */ true,
                allowTint);
    }

    /**
     * Create OmniboxDrawableState representing a site favicon.
     *
     * @param context current context
     * @param bitmap bitmap with decoded site favicon
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forFavIcon(Context context, Bitmap bitmap) {
        return new OmniboxDrawableState(
                new BitmapDrawable(context.getResources(), bitmap),
                /* useRoundedCorners= */ true,
                /* isLarge= */ false,
                /* allowTint= */ false);
    }

    /**
     * Create OmniboxDrawableState with dedicated image decoration.
     *
     * @param context current context
     * @param bitmap dedicated bitmap
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forImage(Context context, Bitmap bitmap) {
        return new OmniboxDrawableState(
                new BitmapDrawable(context.getResources(), bitmap),
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
            boolean useRoundedCorners,
            boolean isLarge,
            boolean allowTint) {
        this.drawable = drawable;
        this.incognitoDrawable = incognitoDrawable;
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
            Drawable drawable, boolean useRoundedCorners, boolean isLarge, boolean allowTint) {
        this(drawable, drawable, useRoundedCorners, isLarge, allowTint);
    }
}
