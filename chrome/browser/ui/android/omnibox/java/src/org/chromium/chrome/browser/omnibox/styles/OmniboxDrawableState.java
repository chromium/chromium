// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Represents graphical decoration for the suggestion components. */
@NullMarked
public class OmniboxDrawableState {
    /** Edge size the decoration should be presented at. */
    @IntDef({IconSize.SMALL, IconSize.MEDIUM, IconSize.LARGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IconSize {
        /** Small icons (e.g. XML fallback icons, favicons). */
        int SMALL = 0;

        /** Medium icons (e.g. natively rasterized vector icons with built-in padding). */
        int MEDIUM = 1;

        /** Large icons (e.g. entity images, answer images). */
        int LARGE = 2;
    }

    /** Embedded drawable object. */
    public final Drawable drawable;

    /** Whether supplied drawable can be tinted. */
    public final boolean allowTint;

    /** Whether drawable should be rounded. */
    public final boolean useRoundedCorners;

    /** Edge size the drawable should be displayed at. */
    public final @IconSize int size;

    /** Resource ID of the drawable, if known. Used for testing. */
    public final @DrawableRes int resourceIdForTesting;

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
                IconSize.LARGE,
                /* allowTint= */ false,
                Resources.ID_NULL);
    }

    /**
     * Create OmniboxDrawableState representing a small fallback icon.
     *
     * @param resourceProvider resource provider
     * @param resourceId resource ID of the drawable
     * @param allowTint whether the icon should be tinted with text color
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forSmallIcon(
            OmniboxResourceProvider resourceProvider,
            @DrawableRes int resourceId,
            boolean allowTint) {
        return new OmniboxDrawableState(
                resourceProvider.getDrawable(resourceId),
                /* useRoundedCorners= */ false,
                IconSize.SMALL,
                allowTint,
                resourceId);
    }

    /**
     * Create OmniboxDrawableState representing a large fallback icon.
     *
     * @param resourceProvider resource provider
     * @param resourceId resource ID of the drawable
     * @param allowTint whether the icon should be tinted with text color
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forLargeIcon(
            OmniboxResourceProvider resourceProvider,
            @DrawableRes int resourceId,
            boolean allowTint) {
        return new OmniboxDrawableState(
                resourceProvider.getDrawable(resourceId),
                /* useRoundedCorners= */ false,
                IconSize.LARGE,
                allowTint,
                resourceId);
    }

    /**
     * Create OmniboxDrawableState representing a site favicon.
     *
     * @param drawable Drawable of the favicon
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forFavIcon(Drawable drawable) {
        return new OmniboxDrawableState(
                drawable,
                /* useRoundedCorners= */ true,
                IconSize.SMALL,
                /* allowTint= */ false,
                Resources.ID_NULL);
    }

    /**
     * Create OmniboxDrawableState with dedicated image decoration.
     *
     * @param drawable dedicated drawable
     * @return newly created OmniboxDrawableState
     */
    public static OmniboxDrawableState forImage(Drawable drawable) {
        return new OmniboxDrawableState(
                drawable,
                /* useRoundedCorners= */ true,
                IconSize.LARGE,
                /* allowTint= */ false,
                Resources.ID_NULL);
    }

    /**
     * Create new OmniboxDrawableState.
     *
     * @param drawable the object to draw
     * @param useRoundedCorners whether to round drawable's corners
     * @param size edge size the drawable should be shown at
     * @param allowTint whether the icon should be tinted with text color
     * @param resourceId resource ID of the drawable, if known
     */
    private OmniboxDrawableState(
            Drawable drawable,
            boolean useRoundedCorners,
            @IconSize int size,
            boolean allowTint,
            @DrawableRes int resourceId) {
        this.drawable = drawable;
        this.useRoundedCorners = useRoundedCorners;
        this.size = size;
        this.allowTint = allowTint;
        this.resourceIdForTesting = resourceId;
    }

    /**
     * Create new OmniboxDrawableState.
     *
     * @param drawable the object to draw
     * @param useRoundedCorners whether to round drawable's corners
     * @param size edge size the drawable should be shown at
     * @param allowTint whether the icon should be tinted with text color
     */
    @VisibleForTesting
    public OmniboxDrawableState(
            Drawable drawable, boolean useRoundedCorners, @IconSize int size, boolean allowTint) {
        this(drawable, useRoundedCorners, size, allowTint, Resources.ID_NULL);
    }
}
