// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;

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
        @NonNull
        private final Drawable mDrawable;
        // TODO(crbug.com/1185382): make mOnClickListener @NonNull
        @Nullable
        private final View.OnClickListener mOnClickListener;
        @Nullable
        private final View.OnLongClickListener mOnLongClickListener;
        @StringRes
        private final int mContentDescriptionResId;
        private final boolean mSupportsTinting;
        @Nullable
        private final IPHCommandBuilder mIPHCommandBuilder;
        @AdaptiveToolbarButtonVariant
        private final int mButtonVariant;

        public ButtonSpec(@NonNull Drawable drawable, @NonNull View.OnClickListener onClickListener,
                @Nullable View.OnLongClickListener onLongClickListener, int contentDescriptionResId,
                boolean supportsTinting, @Nullable IPHCommandBuilder iphCommandBuilder,
                @AdaptiveToolbarButtonVariant int buttonVariant) {
            mDrawable = drawable;
            mOnClickListener = onClickListener;
            mOnLongClickListener = onLongClickListener;
            mContentDescriptionResId = contentDescriptionResId;
            mSupportsTinting = supportsTinting;
            mIPHCommandBuilder = iphCommandBuilder;
            mButtonVariant = buttonVariant;
        }

        public ButtonSpec(@NonNull Drawable drawable, @NonNull View.OnClickListener onClickListener,
                int contentDescriptionResId, boolean supportsTinting,
                @Nullable IPHCommandBuilder iphCommandBuilder,
                @AdaptiveToolbarButtonVariant int buttonVariant) {
            this(drawable, onClickListener, /*onLongClickListener=*/null, contentDescriptionResId,
                    supportsTinting, iphCommandBuilder, buttonVariant);
        }

        public ButtonSpec(Drawable drawable, View.OnClickListener onClickListener,
                int contentDescriptionResId, boolean supportsTinting,
                IPHCommandBuilder iphCommandBuilder) {
            this(drawable, onClickListener, contentDescriptionResId, supportsTinting,
                    iphCommandBuilder, AdaptiveToolbarButtonVariant.UNKNOWN);
        }

        /** Returns the {@link Drawable} for the button icon. */
        @NonNull
        public Drawable getDrawable() {
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

        /** Returns the resource if of the string describing the button. */
        @StringRes
        public int getContentDescriptionResId() {
            return mContentDescriptionResId;
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
        @Nullable
        public IPHCommandBuilder getIPHCommandBuilder() {
            return mIPHCommandBuilder;
        }

        /** Returns the adaptive button variant used for recording metrics. */
        @AdaptiveToolbarButtonVariant
        public int getButtonVariant() {
            return mButtonVariant;
        }
    }
}
