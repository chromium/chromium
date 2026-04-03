// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;

import java.util.Objects;

/** An implementation of the {@link ButtonData}. */
@NullMarked
public class ButtonDataImpl implements ButtonData {
    private boolean mCanShow;
    private boolean mIsEnabled;
    private boolean mShouldShowTextBubble;

    private @SuppressWarnings("NullAway.Init") ButtonSpec mButtonSpec;

    public ButtonDataImpl() {}

    public ButtonDataImpl(
            boolean canShow,
            @Nullable Drawable drawable,
            OnClickListener onClickListener,
            String contentDescription,
            boolean supportsTinting,
            @Nullable IphCommandBuilder iphCommandBuilder,
            boolean isEnabled,
            @AdaptiveToolbarButtonVariant int buttonVariant,
            int tooltipTextResId) {
        this(
                canShow,
                drawable,
                onClickListener,
                contentDescription,
                /* actionChipLabelResId= */ Resources.ID_NULL,
                supportsTinting,
                iphCommandBuilder,
                isEnabled,
                buttonVariant,
                tooltipTextResId);
    }

    public ButtonDataImpl(
            boolean canShow,
            @Nullable Drawable drawable,
            OnClickListener onClickListener,
            String contentDescription,
            @StringRes int actionChipLabelResId,
            boolean supportsTinting,
            @Nullable IphCommandBuilder iphCommandBuilder,
            boolean isEnabled,
            @AdaptiveToolbarButtonVariant int buttonVariant,
            @StringRes int tooltipTextResId) {
        mCanShow = canShow;
        mIsEnabled = isEnabled;
        mButtonSpec =
                new ButtonSpec.Builder(drawable, contentDescription, supportsTinting)
                        .setOnClickListener(onClickListener)
                        .setIphCommandBuilder(iphCommandBuilder)
                        .setButtonVariant(buttonVariant)
                        .setActionChipLabelResId(actionChipLabelResId)
                        .setHoverTooltipTextId(tooltipTextResId)
                        .build();
    }

    @Override
    public boolean canShow() {
        return mCanShow;
    }

    @Override
    public boolean shouldShowTextBubble() {
        return mShouldShowTextBubble;
    }

    @Override
    public boolean isEnabled() {
        return mIsEnabled;
    }

    @Override
    public ButtonSpec getButtonSpec() {
        return mButtonSpec;
    }

    public void setButtonSpec(ButtonSpec buttonSpec) {
        mButtonSpec = buttonSpec;
    }

    public void setCanShow(boolean canShow) {
        mCanShow = canShow;
    }

    public void setShouldShowTextBubble(boolean show) {
        mShouldShowTextBubble = show;
    }

    public void setEnabled(boolean enabled) {
        mIsEnabled = enabled;
    }

    /** Convenience method to update the IPH command builder. */
    public void updateIphCommandBuilder(@Nullable IphCommandBuilder iphCommandBuilder) {
        setButtonSpec(
                new ButtonSpec.Builder(getButtonSpec())
                        .setIphCommandBuilder(iphCommandBuilder)
                        .build());
    }

    /** Convenience method to update the action chip string resource ID. */
    public void updateActionChipResourceId(@StringRes int newActionChipResourceId) {
        setButtonSpec(
                new ButtonSpec.Builder(getButtonSpec())
                        .setActionChipLabelResId(newActionChipResourceId)
                        .build());
    }

    /** Convenience method to update the action chip string resource ID. */
    public void updateDrawable(@Nullable Drawable newDrawable) {
        setButtonSpec(new ButtonSpec.Builder(getButtonSpec()).setDrawable(newDrawable).build());
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof ButtonDataImpl)) {
            return false;
        }
        ButtonDataImpl that = (ButtonDataImpl) o;
        return mCanShow == that.mCanShow
                && mIsEnabled == that.mIsEnabled
                && Objects.equals(mButtonSpec, that.mButtonSpec);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mCanShow, mIsEnabled, mButtonSpec);
    }
}
