// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;

import java.util.Objects;

/** An implementation of the {@link ButtonData}. */
public class ButtonDataImpl implements ButtonData {
    private boolean mCanShow;
    private boolean mIsEnabled;

    private ButtonSpec mButtonSpec;

    public ButtonDataImpl() {}

    public ButtonDataImpl(
            boolean canShow,
            @NonNull Drawable drawable,
            @NonNull OnClickListener onClickListener,
            String contentDescription,
            boolean supportsTinting,
            @Nullable IPHCommandBuilder iphCommandBuilder,
            boolean isEnabled,
            @AdaptiveToolbarButtonVariant int buttonVariant,
            int tooltipTextResId,
            boolean showHoverhighlight) {
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
                tooltipTextResId,
                showHoverhighlight);
    }

    public ButtonDataImpl(
            boolean canShow,
            @NonNull Drawable drawable,
            @NonNull OnClickListener onClickListener,
            String contentDescription,
            @StringRes int actionChipLabelResId,
            boolean supportsTinting,
            @Nullable IPHCommandBuilder iphCommandBuilder,
            boolean isEnabled,
            @AdaptiveToolbarButtonVariant int buttonVariant,
            @StringRes int tooltipTextResId,
            boolean showHoverHighlight) {
        mCanShow = canShow;
        mIsEnabled = isEnabled;
        mButtonSpec =
                new ButtonSpec(
                        drawable,
                        onClickListener,
                        /* onLongClickListener= */ null,
                        contentDescription,
                        supportsTinting,
                        iphCommandBuilder,
                        buttonVariant,
                        actionChipLabelResId,
                        tooltipTextResId,
                        showHoverHighlight);
    }

    @Override
    public boolean canShow() {
        return mCanShow;
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

    public void setEnabled(boolean enabled) {
        mIsEnabled = enabled;
    }

    /** Convenience method to update the IPH command builder. */
    public void updateIPHCommandBuilder(@Nullable IPHCommandBuilder iphCommandBuilder) {
        ButtonSpec currentSpec = getButtonSpec();
        ButtonSpec newSpec =
                new ButtonSpec(
                        currentSpec.getDrawable(),
                        currentSpec.getOnClickListener(),
                        currentSpec.getOnLongClickListener(),
                        currentSpec.getContentDescription(),
                        currentSpec.getSupportsTinting(),
                        iphCommandBuilder,
                        currentSpec.getButtonVariant(),
                        currentSpec.getActionChipLabelResId(),
                        currentSpec.getHoverTooltipTextId(),
                        currentSpec.getShouldShowHoverHighlight());
        setButtonSpec(newSpec);
    }

    /** Convenience method to update the action chip string resource ID. */
    public void updateActionChipResourceId(@StringRes int newActionChipResourceId) {
        ButtonSpec currentSpec = getButtonSpec();
        ButtonSpec newSpec =
                new ButtonSpec(
                        currentSpec.getDrawable(),
                        currentSpec.getOnClickListener(),
                        currentSpec.getOnLongClickListener(),
                        currentSpec.getContentDescription(),
                        currentSpec.getSupportsTinting(),
                        currentSpec.getIPHCommandBuilder(),
                        currentSpec.getButtonVariant(),
                        newActionChipResourceId,
                        currentSpec.getHoverTooltipTextId(),
                        currentSpec.getShouldShowHoverHighlight());
        setButtonSpec(newSpec);
    }

    /** Convenience method to update the action chip string resource ID. */
    public void updateDrawable(Drawable newDrawable) {
        ButtonSpec currentSpec = getButtonSpec();
        ButtonSpec newSpec =
                new ButtonSpec(
                        newDrawable,
                        currentSpec.getOnClickListener(),
                        currentSpec.getOnLongClickListener(),
                        currentSpec.getContentDescription(),
                        currentSpec.getSupportsTinting(),
                        currentSpec.getIPHCommandBuilder(),
                        currentSpec.getButtonVariant(),
                        currentSpec.getActionChipLabelResId(),
                        currentSpec.getHoverTooltipTextId(),
                        currentSpec.getShouldShowHoverHighlight());
        setButtonSpec(newSpec);
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
