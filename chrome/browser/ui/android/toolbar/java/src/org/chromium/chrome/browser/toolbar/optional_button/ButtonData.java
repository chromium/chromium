// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;

import java.util.Objects;

/**
 * Data needed to show an optional toolbar button.
 *
 * @see ButtonDataProvider
 */
@NullMarked
public interface ButtonData {
    /** Returns {@code true} when the {@link ButtonDataProvider} wants to show a button. */
    boolean canShow();

    /** Returns {@code true} if the button is supposed to be enabled and clickable. */
    boolean isEnabled();

    /**
     * Returns {@code true} if the button with a chip string should show a text bubble instead of
     * expansion/collapse animation.
     */
    boolean shouldShowTextBubble();

    /**
     * Returns a {@link ButtonSpec} describing button properties which don't change often. When
     * feasible, a {@link ButtonDataProvider} should prefer to reuse a single {@code ButtonSpec}
     * instance.
     */
    ButtonSpec getButtonSpec();

    /** A set of button properties which are not expected to change values often. */
    final class ButtonSpec {
        public static final int INVALID_TOOLTIP_TEXT_ID = 0;
        private final @Nullable Drawable mDrawable;
        // TODO(crbug.com/40753109): make mOnClickListener
        private final View.OnClickListener mOnClickListener;
        private final View.@Nullable OnLongClickListener mOnLongClickListener;
        private final String mContentDescription;
        private final boolean mSupportsTinting;
        private final @Nullable IphCommandBuilder mIphCommandBuilder;
        @AdaptiveToolbarButtonVariant private final int mButtonVariant;
        private final boolean mIsDynamicAction;
        @StringRes private final int mActionChipLabelResId;
        @StringRes private final int mTooltipTextResId;
        private final boolean mHasErrorBadge;
        private final boolean mIsChecked;

        public ButtonSpec(
                @Nullable Drawable drawable,
                View.OnClickListener onClickListener,
                View.@Nullable OnLongClickListener onLongClickListener,
                String contentDescription,
                boolean supportsTinting,
                @Nullable IphCommandBuilder iphCommandBuilder,
                @AdaptiveToolbarButtonVariant int buttonVariant,
                int actionChipLabelResId,
                int tooltipTextResId,
                boolean hasErrorBadge) {
            this(
                    drawable,
                    onClickListener,
                    onLongClickListener,
                    contentDescription,
                    supportsTinting,
                    iphCommandBuilder,
                    buttonVariant,
                    actionChipLabelResId,
                    tooltipTextResId,
                    hasErrorBadge,
                    /* isChecked= */ false);
        }

        public ButtonSpec(
                @Nullable Drawable drawable,
                View.OnClickListener onClickListener,
                View.@Nullable OnLongClickListener onLongClickListener,
                String contentDescription,
                boolean supportsTinting,
                @Nullable IphCommandBuilder iphCommandBuilder,
                @AdaptiveToolbarButtonVariant int buttonVariant,
                int actionChipLabelResId,
                int tooltipTextResId,
                boolean hasErrorBadge,
                boolean isChecked) {
            mDrawable = drawable;
            mOnClickListener = onClickListener;
            mOnLongClickListener = onLongClickListener;
            mContentDescription = contentDescription;
            mSupportsTinting = supportsTinting;
            mIphCommandBuilder = iphCommandBuilder;
            mButtonVariant = buttonVariant;
            mIsDynamicAction = AdaptiveToolbarFeatures.isDynamicAction(mButtonVariant);
            mActionChipLabelResId = actionChipLabelResId;
            mTooltipTextResId = tooltipTextResId;
            mHasErrorBadge = hasErrorBadge;
            mIsChecked = isChecked;
        }

        /** Returns the {@link Drawable} for the button icon. */
        public @Nullable Drawable getDrawable() {
            return mDrawable;
        }

        /** Returns the {@link View.OnClickListener} used on the button. */
        public View.OnClickListener getOnClickListener() {
            return mOnClickListener;
        }

        /** Returns an optional {@link View.OnLongClickListener} used on the button. */
        public View.@Nullable OnLongClickListener getOnLongClickListener() {
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
        public @Nullable IphCommandBuilder getIphCommandBuilder() {
            return mIphCommandBuilder;
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
         * Returns {@code true} if the button has an error badge. False otherwise. The button's
         * height is increased to accommodate the larger icon when an error badge is present.
         */
        public boolean hasErrorBadge() {
            return mHasErrorBadge;
        }

        /**
         * Returns true if the button is a "checked" state. Currently, price tracking is the only
         * action with a "checked" state. For price tracking, returns true if the price is being
         * tracked and false otherwise.
         */
        public boolean isChecked() {
            return mIsChecked;
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
                    && mIsChecked == that.mIsChecked
                    && Objects.equals(mDrawable, that.mDrawable)
                    && Objects.equals(mOnClickListener, that.mOnClickListener)
                    && Objects.equals(mOnLongClickListener, that.mOnLongClickListener)
                    && Objects.equals(mContentDescription, that.mContentDescription)
                    && Objects.equals(mIphCommandBuilder, that.mIphCommandBuilder);
        }

        @Override
        public int hashCode() {
            return Objects.hash(
                    mDrawable,
                    mOnClickListener,
                    mOnLongClickListener,
                    mContentDescription,
                    mSupportsTinting,
                    mIphCommandBuilder,
                    mButtonVariant,
                    mIsDynamicAction,
                    mActionChipLabelResId);
        }
    }
}
