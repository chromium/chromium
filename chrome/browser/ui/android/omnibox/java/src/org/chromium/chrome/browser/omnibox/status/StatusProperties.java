// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.util.ObjectsCompat;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Model properties for the Status. */
public class StatusProperties {
    // TODO(wylieb): Investigate the case where we only want to swap the tint (if any).
    /** Encapsulates an icon and tint to allow atomic drawable updates for StatusView. */
    public static class StatusIconResource {
        private @DrawableRes Integer mIconRes;
        private @ColorRes int mTint;
        private String mIconIdentifier;
        private Bitmap mBitmap;
        private Drawable mDrawable;
        private @StatusView.IconTransitionType int mIconTransitionType =
                StatusView.IconTransitionType.CROSSFADE;
        private Runnable mCallback;

        /** Constructor for a custom drawable. */
        public StatusIconResource(Drawable drawable) {
            mDrawable = drawable;
        }

        /** Constructor for a custom drawable with identifier. */
        public StatusIconResource(Drawable drawable, String iconIdentifier) {
            mDrawable = drawable;
            mIconIdentifier = iconIdentifier;
        }

        /** Constructor for a custom bitmap. */
        public StatusIconResource(String iconIdentifier, Bitmap bitmap, @ColorRes int tint) {
            mIconIdentifier = iconIdentifier;
            mBitmap = bitmap;
            mTint = tint;
        }

        /** Constructor for an Android resource. */
        public StatusIconResource(@DrawableRes int iconRes, @ColorRes int tint) {
            mIconRes = iconRes;
            mTint = tint;
        }

        /**
         * @return The tint associated with this resource.
         */
        @ColorRes
        int getTint() {
            return mTint;
        }

        /**
         * @return The icon res.
         */
        @DrawableRes
        int getIconResForTesting() {
            if (mIconRes == null) return 0;
            return mIconRes;
        }

        /** Set the animation transition type for this icon. */
        void setTransitionType(@StatusView.IconTransitionType int type) {
            mIconTransitionType = type;
        }

        /**
         * @return The animation transition type for this icon.
         */
        @StatusView.IconTransitionType
        int getTransitionType() {
            return mIconTransitionType;
        }

        /**
         * @return The {@link Drawable} for this StatusIconResource.
         */
        Drawable getDrawable(Context context, Resources resources) {
            if (mBitmap != null) {
                Drawable drawable = new BitmapDrawable(resources, mBitmap);
                if (mTint != 0) {
                    DrawableCompat.setTintList(
                            drawable, AppCompatResources.getColorStateList(context, mTint));
                }
                return drawable;
            } else if (mIconRes != null) {
                if (mTint == 0) {
                    return AppCompatResources.getDrawable(context, mIconRes);
                }
                return UiUtils.getTintedDrawable(context, mIconRes, mTint);
            } else if (mDrawable != null) {
                return mDrawable;
            } else {
                return null;
            }
        }

        /**
         * @return The icon identifier, used for testing.
         */
        @Nullable
        String getIconIdentifierForTesting() {
            return mIconIdentifier;
        }

        @Override
        public boolean equals(@Nullable Object other) {
            if (!(other instanceof StatusIconResource)) return false;

            StatusIconResource otherResource = (StatusIconResource) other;
            if (mTint != otherResource.mTint) return false;
            if (!ObjectsCompat.equals(mIconRes, otherResource.mIconRes)) return false;
            if (mBitmap != otherResource.mBitmap) return false;
            if (mDrawable != otherResource.mDrawable) return false;

            return true;
        }

        /**
         * Sets the callback to be run after this icon has been set.
         *
         * @param callback The Runnable to be called. Only works for the ROTATE transition and
         *     called if the animation has run to completion.
         */
        void setAnimationFinishedCallback(Runnable callback) {
            mCallback = callback;
        }

        /**
         * @return the callback to be run after this icon has been set, if any.
         */
        @Nullable
        Runnable getAnimationFinishedCallback() {
            return mCallback;
        }
    }

    /**
     * Encapsulates a permission icon for StatusView. Adds a circle background and icon color
     * highlight.
     */
    static class PermissionIconResource extends StatusIconResource {
        // Size of the drawable in the omnibox. This class creates a circle of this size
        // and draw a icon of size INNER_ICON_DP centered in the circle.
        public static final int OMNIBOX_ICON_DP = 24;
        public static final int INNER_ICON_DP = 20;

        private boolean mIsIncognito;

        PermissionIconResource(Drawable drawable, boolean isIncognito) {
            super(drawable);
            mIsIncognito = isIncognito;
        }

        PermissionIconResource(Drawable drawable, boolean isIncognito, String iconIdentifier) {
            super(drawable, iconIdentifier);
            mIsIncognito = isIncognito;
        }

        /** Returns a {@link Drawable} for this StatusIconResource. */
        @Override
        Drawable getDrawable(Context context, Resources resources) {
            Drawable icon = super.getDrawable(context, resources);
            if (icon == null) {
                return null;
            }
            assert icon.getIntrinsicWidth() == icon.getIntrinsicHeight();
            int width = ViewUtils.dpToPx(context, OMNIBOX_ICON_DP);
            Bitmap bitmap = Bitmap.createBitmap(width, width, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            drawCircleBackground(canvas, context);
            drawCenteredIcon(context, canvas, icon);
            return new BitmapDrawable(resources, bitmap);
        }

        /** Draws the provided icon at INNER_ICON_DP on the canvas. */
        private void drawCenteredIcon(Context context, Canvas canvas, Drawable icon) {
            int width = canvas.getWidth();
            int iconWidth = ViewUtils.dpToPx(context, INNER_ICON_DP);
            int boundOffset = (width - iconWidth) / 2;
            icon.setBounds(
                    boundOffset, boundOffset, boundOffset + iconWidth, boundOffset + iconWidth);
            icon.draw(canvas);
        }

        /** Draws a circle background on canvas. */
        private void drawCircleBackground(Canvas canvas, Context context) {
            float radius = 0.5f * canvas.getWidth();
            Paint paint = new Paint();
            // Use the dark mode color if in incognito mode.
            final @ColorInt int color =
                    mIsIncognito
                            ? context.getColor(R.color.toolbar_background_primary_dark)
                            : SemanticColorUtils.getToolbarBackgroundPrimary(context);
            paint.setColor(color);
            paint.setAntiAlias(true);
            canvas.drawCircle(radius, radius, radius, paint);
        }
    }

    /** Alpha of the entire StatusView container. */
    static final WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();

    /** Whether animations are turned on. */
    static final WritableBooleanPropertyKey ANIMATIONS_ENABLED = new WritableBooleanPropertyKey();

    /** Whether the incognito badge is visible. */
    static final WritableBooleanPropertyKey INCOGNITO_BADGE_VISIBLE =
            new WritableBooleanPropertyKey();

    /** The status separator color. */
    static final WritableIntPropertyKey SEPARATOR_COLOR = new WritableIntPropertyKey();

    /** Whether the icon is shown. */
    static final WritableBooleanPropertyKey SHOW_STATUS_ICON = new WritableBooleanPropertyKey();

    /** Whether the icon background is shown. */
    static final WritableBooleanPropertyKey SHOW_STATUS_ICON_BACKGROUND =
            new WritableBooleanPropertyKey();

    /** The handler of status click events. */
    static final WritableObjectPropertyKey<View.OnClickListener> STATUS_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The accessibility string shown upon a long click. */
    static final WritableIntPropertyKey STATUS_ACCESSIBILITY_TOAST_RES =
            new WritableIntPropertyKey();

    /** The accessibility description read for double tab upon a click on status view. */
    static final WritableIntPropertyKey STATUS_ACCESSIBILITY_DOUBLE_TAP_DESCRIPTION_RES =
            new WritableIntPropertyKey();

    /** Alpha of the icon. */
    static final WritableFloatPropertyKey STATUS_ICON_ALPHA = new WritableFloatPropertyKey();

    /** The string resource used for the description for security icon. */
    static final WritableIntPropertyKey STATUS_ICON_DESCRIPTION_RES = new WritableIntPropertyKey();

    /** The icon resource. */
    static final WritableObjectPropertyKey<StatusIconResource> STATUS_ICON_RESOURCE =
            new WritableObjectPropertyKey<>();

    /** The StatusView tooltip text resource. */
    static final WritableIntPropertyKey STATUS_VIEW_TOOLTIP_TEXT = new WritableIntPropertyKey();

    /** The StatusView hover highlight resource. */
    static final WritableIntPropertyKey STATUS_VIEW_HOVER_HIGHLIGHT = new WritableIntPropertyKey();

    /** The x translation of the status view. */
    static final WritableFloatPropertyKey TRANSLATION_X = new WritableFloatPropertyKey();

    /** Text color of the verbose status text field. */
    static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_COLOR = new WritableIntPropertyKey();

    /** The string resource used for the content of the verbose status text field. */
    static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_STRING_RES =
            new WritableIntPropertyKey();

    /** Whether verbose status text field is visible. */
    static final WritableBooleanPropertyKey VERBOSE_STATUS_TEXT_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Specifies width of the verbose status text field. */
    static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_WIDTH = new WritableIntPropertyKey();

    /**
     * Whether the status view is shown. This is different from SHOW_STATUS_ICON, which is
     * responsible for whether the icon sub-view is shown or not and is managed independently.
     */
    static final WritableBooleanPropertyKey SHOW_STATUS_VIEW = new WritableBooleanPropertyKey();

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ALPHA,
                ANIMATIONS_ENABLED,
                INCOGNITO_BADGE_VISIBLE,
                SEPARATOR_COLOR,
                SHOW_STATUS_ICON,
                SHOW_STATUS_ICON_BACKGROUND,
                SHOW_STATUS_VIEW,
                STATUS_CLICK_LISTENER,
                STATUS_ACCESSIBILITY_TOAST_RES,
                STATUS_ACCESSIBILITY_DOUBLE_TAP_DESCRIPTION_RES,
                STATUS_ICON_ALPHA,
                STATUS_ICON_DESCRIPTION_RES,
                STATUS_ICON_RESOURCE,
                STATUS_VIEW_TOOLTIP_TEXT,
                STATUS_VIEW_HOVER_HIGHLIGHT,
                TRANSLATION_X,
                VERBOSE_STATUS_TEXT_COLOR,
                VERBOSE_STATUS_TEXT_STRING_RES,
                VERBOSE_STATUS_TEXT_VISIBLE,
                VERBOSE_STATUS_TEXT_WIDTH,
            };

    private StatusProperties() {}
}
