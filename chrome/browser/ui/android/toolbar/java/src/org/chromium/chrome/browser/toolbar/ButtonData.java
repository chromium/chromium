// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;

import java.util.Objects;

/**
 * Data needed to show an optional toolbar button.
 *
 * @see ButtonDataProvider
 */
public interface ButtonData {
    /** Returns {@code true} when the {@link ButtonDataProvider} wants to show a button. */
    boolean canShow();

    /** Returns {@code true} if the button is supposed to be enabled and clickable. */
    boolean isEnabled();

    /**
     * Returns a {@link ButtonSpec} describing button properties which don't change often. When
     * feasible, a {@link ButtonDataProvider} should prefer to reuse a single {@code ButtonSpec}
     * instance.
     */
    ButtonSpec getButtonSpec();

    /** A set of button properties which are not expected to change values often. */
    final class ButtonSpec {
        public static final int INVALID_TOOLTIP_TEXT_ID = 0;
        @NonNull private final Drawable mDrawable;
        // TODO(crbug.com/40753109): make mOnClickListener @NonNull
        @Nullable private final View.OnClickListener mOnClickListener;
        @Nullable private final View.OnLongClickListener mOnLongClickListener;
        private final String mContentDescription;
        private final boolean mSupportsTinting;
        @Nullable private final IPHCommandBuilder mIPHCommandBuilder;
        @AdaptiveToolbarButtonVariant private final int mButtonVariant;
        private final boolean mIsDynamicAction;
        @StringRes private final int mActionChipLabelResId;
        private final boolean mShowHoverHighlight;
        @StringRes private final int mTooltipTextResId;

        public ButtonSpec(
                @NonNull Drawable drawable,
                @NonNull View.OnClickListener onClickListener,
                @Nullable View.OnLongClickListener onLongClickListener,
                String contentDescription,
                boolean supportsTinting,
                @Nullable IPHCommandBuilder iphCommandBuilder,
                @AdaptiveToolbarButtonVariant int buttonVariant,
                int actionChipLabelResId,
                int tooltipTextResId,
                boolean showHoverHighlight) {
            mDrawable = drawable;
            mOnClickListener = onClickListener;
            mOnLongClickListener = onLongClickListener;
            mContentDescription = contentDescription;
            mSupportsTinting = supportsTinting;
            mIPHCommandBuilder = iphCommandBuilder;
            mButtonVariant = buttonVariant;
            mIsDynamicAction = AdaptiveToolbarFeatures.isDynamicAction(mButtonVariant);
            mActionChipLabelResId = actionChipLabelResId;
            mTooltipTextResId = tooltipTextResId;
            mShowHoverHighlight = showHoverHighlight;
        }

        /** Returns the {@link Drawable} for the button icon. */
        public @NonNull Drawable getDrawable() {
            return mDrawable;
        }

        /** Returns the {@link View.OnClickListener} used on the button. */
        @NonNull
        public View.OnClickListener getOnClickListener() {
            return mOnClickListener;
        }

        /** Returns an optional {@link View.OnLongClickListener} used on the button. */
        @NonNull
        public View.OnLongClickListener getOnLongClickListener() {
            return mOnLongClickListener;
        }

        /** Returns the string describing the button. */
        public String getContentDescription() {
            return mContentDescription;
        }

        /** Returns the resource ID of the string for the button's action chip label. */
        public @StringRes int getActionChipLabelResId() {
            return mActionChipLabelResId;
        }

        /** Returns {@code true} if the button supports tinting. */
        public boolean getSupportsTinting() {
            return mSupportsTinting;
        }

        /**
         * Returns the builder used to show IPH on the button as it's shown. This should include at
         * a minimum the feature name, content string, and accessibility text, but not the anchor
         * view.
         */
        public @Nullable IPHCommandBuilder getIPHCommandBuilder() {
            return mIPHCommandBuilder;
        }

        /** Returns the adaptive button variant used for recording metrics. */
        public @AdaptiveToolbarButtonVariant int getButtonVariant() {
            return mButtonVariant;
        }

        /** Returns {@code true} if the button is a contextual page action. False otherwise. */
        public boolean isDynamicAction() {
            return mIsDynamicAction;
        }

        /**
         * Get hover state tooltip text for optional toolbar buttons(e.g. share, voice search, new
         * tab and profile).
         */
        public @StringRes int getHoverTooltipTextId() {
            return mTooltipTextResId;
        }

        /**
         * Show hover highlight for optional toolbar buttons(e.g. share, voice search, new tab and
         * profile).
         */
        public boolean getShouldShowHoverHighlight() {
            return mShowHoverHighlight;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (!(o instanceof ButtonSpec)) {
                return false;
            }
            ButtonSpec that = (ButtonSpec) o;
            return mSupportsTinting == that.mSupportsTinting
                    && mButtonVariant == that.mButtonVariant
                    && mIsDynamicAction == that.mIsDynamicAction
                    && mActionChipLabelResId == that.mActionChipLabelResId
                    && Objects.equals(mDrawable, that.mDrawable)
                    && Objects.equals(mOnClickListener, that.mOnClickListener)
                    && Objects.equals(mOnLongClickListener, that.mOnLongClickListener)
                    && Objects.equals(mContentDescription, that.mContentDescription)
                    && Objects.equals(mIPHCommandBuilder, that.mIPHCommandBuilder);
        }

        @Override
        public int hashCode() {
            return Objects.hash(
                    mDrawable,
                    mOnClickListener,
                    mOnLongClickListener,
                    mContentDescription,
                    mSupportsTinting,
                    mIPHCommandBuilder,
                    mButtonVariant,
                    mIsDynamicAction,
                    mActionChipLabelResId);
        }
    }
}
