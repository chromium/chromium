// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnLongClickListener;

import androidx.annotation.AttrRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
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
    /** Default delay for collapsing the action chip. */
    int DEFAULT_ACTION_CHIP_DELAY_MS = 3000;

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
        private final @Nullable Drawable mCollapsedDrawable;
        // TODO(crbug.com/40753109): make mOnClickListener
        private final @Nullable View.OnClickListener mOnClickListener;
        private final @Nullable OnLongClickListener mOnLongClickListener;
        private final String mContentDescription;
        private final boolean mSupportsTinting;
        private final @Nullable IphCommandBuilder mIphCommandBuilder;
        @AdaptiveToolbarButtonVariant private final int mButtonVariant;
        private final boolean mIsDynamicAction;
        private final @StringRes int mActionChipLabelResId;
        private final @StringRes int mTooltipTextResId;
        private final boolean mHasErrorBadge;
        private final boolean mIsChecked;
        private final boolean mShouldSuppressCpa;
        private final int mActionChipCollapseDelayMs;
        private final @AttrRes int mActionChipBackgroundColorResId;
        private final @AttrRes int mActionChipTextColorResId;

        private ButtonSpec(
                @Nullable Drawable drawable,
                @Nullable Drawable collapsedDrawable,
                @Nullable View.OnClickListener onClickListener,
                @Nullable OnLongClickListener onLongClickListener,
                String contentDescription,
                boolean supportsTinting,
                @Nullable IphCommandBuilder iphCommandBuilder,
                @AdaptiveToolbarButtonVariant int buttonVariant,
                int actionChipLabelResId,
                int tooltipTextResId,
                boolean hasErrorBadge,
                boolean isChecked,
                boolean shouldSuppressCpa,
                int actionChipCollapseDelayMs,
                @AttrRes int actionChipBackgroundColorResId,
                @AttrRes int actionChipTextColorResId) {
            mDrawable = drawable;
            mCollapsedDrawable = collapsedDrawable;
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
            mShouldSuppressCpa = shouldSuppressCpa;
            mActionChipCollapseDelayMs = actionChipCollapseDelayMs;
            mActionChipBackgroundColorResId = actionChipBackgroundColorResId;
            mActionChipTextColorResId = actionChipTextColorResId;
        }

        /** Builder for {@link ButtonSpec}. */
        public static class Builder {
            private @Nullable Drawable mDrawable;
            private @Nullable Drawable mCollapsedDrawable;
            private @Nullable View.OnClickListener mOnClickListener;
            private @Nullable OnLongClickListener mOnLongClickListener;
            private String mContentDescription;
            private boolean mSupportsTinting;
            private @Nullable IphCommandBuilder mIphCommandBuilder;
            private @AdaptiveToolbarButtonVariant int mButtonVariant =
                    AdaptiveToolbarButtonVariant.UNKNOWN;
            private @StringRes int mActionChipLabelResId = INVALID_TOOLTIP_TEXT_ID;
            private @StringRes int mTooltipTextResId = INVALID_TOOLTIP_TEXT_ID;
            private boolean mHasErrorBadge;
            private boolean mIsChecked;
            private boolean mShouldSuppressCpa;
            private int mActionChipCollapseDelayMs = DEFAULT_ACTION_CHIP_DELAY_MS;
            private @AttrRes int mActionChipBackgroundColorResId = Resources.ID_NULL;
            private @AttrRes int mActionChipTextColorResId = Resources.ID_NULL;

            /**
             * Creates a new {@link Builder} with the required properties.
             *
             * @param drawable The {@link Drawable} for the button icon.
             * @param contentDescription The string describing the button.
             * @param supportsTinting Whether the button supports tinting.
             */
            public Builder(
                    @Nullable Drawable drawable,
                    String contentDescription,
                    boolean supportsTinting) {
                mDrawable = drawable;
                mContentDescription = contentDescription;
                mSupportsTinting = supportsTinting;
            }

            /**
             * Creates a new {@link Builder} from an existing {@link ButtonSpec}.
             *
             * @param buttonSpec The existing {@link ButtonSpec} to copy properties from.
             */
            public Builder(ButtonSpec buttonSpec) {
                mDrawable = buttonSpec.mDrawable;
                mCollapsedDrawable = buttonSpec.mCollapsedDrawable;
                mOnClickListener = buttonSpec.mOnClickListener;
                mOnLongClickListener = buttonSpec.mOnLongClickListener;
                mContentDescription = buttonSpec.mContentDescription;
                mSupportsTinting = buttonSpec.mSupportsTinting;
                mIphCommandBuilder = buttonSpec.mIphCommandBuilder;
                mButtonVariant = buttonSpec.mButtonVariant;
                mActionChipLabelResId = buttonSpec.mActionChipLabelResId;
                mTooltipTextResId = buttonSpec.mTooltipTextResId;
                mHasErrorBadge = buttonSpec.mHasErrorBadge;
                mIsChecked = buttonSpec.mIsChecked;
                mShouldSuppressCpa = buttonSpec.mShouldSuppressCpa;
                mActionChipCollapseDelayMs = buttonSpec.mActionChipCollapseDelayMs;
                mActionChipBackgroundColorResId = buttonSpec.mActionChipBackgroundColorResId;
                mActionChipTextColorResId = buttonSpec.mActionChipTextColorResId;
            }

            public Builder setDrawable(@Nullable Drawable drawable) {
                mDrawable = drawable;
                return this;
            }

            public Builder setCollapsedDrawable(@Nullable Drawable collapsedDrawable) {
                mCollapsedDrawable = collapsedDrawable;
                return this;
            }

            public Builder setOnClickListener(@Nullable View.OnClickListener onClickListener) {
                mOnClickListener = onClickListener;
                return this;
            }

            public Builder setOnLongClickListener(
                    @Nullable OnLongClickListener onLongClickListener) {
                mOnLongClickListener = onLongClickListener;
                return this;
            }

            public Builder setContentDescription(String contentDescription) {
                mContentDescription = contentDescription;
                return this;
            }

            public Builder setSupportsTinting(boolean supportsTinting) {
                mSupportsTinting = supportsTinting;
                return this;
            }

            public Builder setIphCommandBuilder(@Nullable IphCommandBuilder iphCommandBuilder) {
                mIphCommandBuilder = iphCommandBuilder;
                return this;
            }

            public Builder setButtonVariant(@AdaptiveToolbarButtonVariant int buttonVariant) {
                mButtonVariant = buttonVariant;
                return this;
            }

            public Builder setActionChipLabelResId(@StringRes int actionChipLabelResId) {
                mActionChipLabelResId = actionChipLabelResId;
                return this;
            }

            public Builder setHoverTooltipTextId(@StringRes int tooltipTextResId) {
                mTooltipTextResId = tooltipTextResId;
                return this;
            }

            public Builder setHasErrorBadge(boolean hasErrorBadge) {
                mHasErrorBadge = hasErrorBadge;
                return this;
            }

            public Builder setIsChecked(boolean isChecked) {
                mIsChecked = isChecked;
                return this;
            }

            public Builder setShouldSuppressCpa(boolean shouldSuppressCpa) {
                mShouldSuppressCpa = shouldSuppressCpa;
                return this;
            }

            public Builder setActionChipCollapseDelayMs(int actionChipCollapseDelayMs) {
                mActionChipCollapseDelayMs = actionChipCollapseDelayMs;
                return this;
            }

            public Builder setActionChipBackgroundColorResId(
                    @AttrRes int actionChipBackgroundColorResId) {
                mActionChipBackgroundColorResId = actionChipBackgroundColorResId;
                return this;
            }

            public Builder setActionChipTextColorResId(@AttrRes int actionChipTextColorResId) {
                mActionChipTextColorResId = actionChipTextColorResId;
                return this;
            }

            public ButtonSpec build() {
                return new ButtonSpec(
                        mDrawable,
                        mCollapsedDrawable,
                        mOnClickListener,
                        mOnLongClickListener,
                        mContentDescription,
                        mSupportsTinting,
                        mIphCommandBuilder,
                        mButtonVariant,
                        mActionChipLabelResId,
                        mTooltipTextResId,
                        mHasErrorBadge,
                        mIsChecked,
                        mShouldSuppressCpa,
                        mActionChipCollapseDelayMs,
                        mActionChipBackgroundColorResId,
                        mActionChipTextColorResId);
            }
        }

        /** Returns the {@link Drawable} for the button icon. */
        public @Nullable Drawable getDrawable() {
            return mDrawable;
        }

        /**
         * Returns the {@link Drawable} for the button icon when collapsed (e.g., after an action
         * chip collapses), as opposed to the regular drawable which is used when the button is
         * first shown or expanded. If this is null then the regular drawable is used for all
         * states.
         */
        public @Nullable Drawable getCollapsedDrawable() {
            return mCollapsedDrawable;
        }

        /** Returns the {@link View.OnClickListener} used on the button. */
        public @Nullable View.OnClickListener getOnClickListener() {
            return mOnClickListener;
        }

        /** Returns an optional {@link View.OnLongClickListener} used on the button. */
        public @Nullable OnLongClickListener getOnLongClickListener() {
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

        /** Returns {@code true} if the button should suppress Contextual Page Actions. */
        public boolean shouldSuppressCpa() {
            return mShouldSuppressCpa;
        }

        /**
         * Returns the delay for collapsing the action chip in milliseconds. The default value is
         * 3000ms.
         */
        public int getActionChipCollapseDelayMs() {
            return mActionChipCollapseDelayMs;
        }

        /**
         * Returns the resource ID of the attribute for the action chip background color. This is
         * only applied when the action chip is on the expanding and expanded states. It will use
         * the default colors otherwise.
         */
        public @AttrRes int getActionChipBackgroundColorResId() {
            return mActionChipBackgroundColorResId;
        }

        /**
         * Returns the resource ID of the attribute for the action chip text color. This is only
         * applied when the action chip is on the expanding and expanded states. It will use the
         * default colors otherwise.
         */
        public @AttrRes int getActionChipTextColorResId() {
            return mActionChipTextColorResId;
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
                    && mTooltipTextResId == that.mTooltipTextResId
                    && mHasErrorBadge == that.mHasErrorBadge
                    && mIsChecked == that.mIsChecked
                    && mShouldSuppressCpa == that.mShouldSuppressCpa
                    && mActionChipCollapseDelayMs == that.mActionChipCollapseDelayMs
                    && mActionChipBackgroundColorResId == that.mActionChipBackgroundColorResId
                    && mActionChipTextColorResId == that.mActionChipTextColorResId
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
                    mActionChipLabelResId,
                    mTooltipTextResId,
                    mHasErrorBadge,
                    mIsChecked,
                    mShouldSuppressCpa,
                    mActionChipCollapseDelayMs,
                    mActionChipBackgroundColorResId,
                    mActionChipTextColorResId);
        }
    }
}
