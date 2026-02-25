// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.CONTENT_DESC;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.ICON;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.ON_CLICK;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.TEXT;

import android.animation.Animator;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AttrUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collection;

/** Manager that manages showing and hiding omnibox chips. */
@NullMarked
public class OmniboxChipManager {
    /** Callback interface to get notified when the chip is hidden or shown. */
    public interface ChipCallback {
        /** Called when the chip is hidden because there is no more space for it. */
        void onChipHidden();

        /** Called when the chip is shown. */
        void onChipShown();
    }

    @IntDef({
        VisibilityState.UNINITIALIZED,
        VisibilityState.HIDDEN,
        VisibilityState.COLLAPSED,
        VisibilityState.EXPANDED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface VisibilityState {
        /** The chip hasn't been shown or hidden yet. */
        int UNINITIALIZED = 0;

        /** The chip is not shown. */
        int HIDDEN = 1;

        /** The chip is shown in its collapsed state. */
        int COLLAPSED = 2;

        /** The chip is shown in its expanded state. */
        int EXPANDED = 3;
    }

    /**
     * Implementation of {@link ToolbarWidthConsumer} for the collapsed (icon only) state that
     * corresponds to `ToolbarComponentId.OMNIBOX_CHIP_COLLAPSED`. This is the first of the two
     * consumers to be called for this component. This consumer decides whether there is enough
     * space to show the chip at all.
     */
    private class CollapsedToolbarWidthConsumer implements ToolbarWidthConsumer {
        @Override
        public boolean isVisible() {
            return mChip != null && mChipVisibilityState != VisibilityState.HIDDEN;
        }

        @Override
        public int updateVisibility(int availableWidth) {
            if (mChip == null) return 0;
            if (availableWidth < mCollapsedWidth) {
                if (mChipVisibilityState != VisibilityState.HIDDEN) {
                    if (mChipVisibilityState != VisibilityState.UNINITIALIZED) {
                        // If we're initializing as hidden, we don't need to call the callback.
                        // TODO(crbug.com/450253146): This can be done in the view binder at the
                        // point we actually set the view visibility.
                        assumeNonNull(mChipCallback).onChipHidden();
                    }
                    mChipVisibilityState = VisibilityState.HIDDEN;
                    mChip.setAvailableWidth(0);
                }

                // Consume all the width to prevent any smaller components from showing.
                return availableWidth;
            }

            // We're showing the chip when it was hidden before. It may become expanded later, but
            // that doesn't change the fact that it's shown now.
            if (mChipVisibilityState == VisibilityState.UNINITIALIZED
                    || mChipVisibilityState == VisibilityState.HIDDEN) {
                mChipVisibilityState = VisibilityState.COLLAPSED;
                assumeNonNull(mChipCallback).onChipShown();
            }

            return mCollapsedWidth;
        }

        @Override
        public int updateVisibilityWithAnimation(
                int availableWidth, Collection<Animator> animators) {
            return updateVisibility(availableWidth);
        }
    }

    /**
     * Implementation of {@link ToolbarWidthConsumer} for the expanded state that corresponds to
     * `ToolbarComponentId.OMNIBOX_CHIP_EXPANDED`. This is the second of the two consumers to be
     * called for this component. This consumer decides whether there is enough space in addition to
     * the collapsed state width to show the chip in its expanded state. If we don't have at least
     * `mMinExpandedWidth` (including the collapsed width), we don't bother showing since we won't
     * be able to show much of the text. At the same time, we don't want the expanded chip to be
     * wider than `mMaxExpandedWidth` total.
     */
    private class ExpandedToolbarWidthConsumer implements ToolbarWidthConsumer {
        @Override
        public boolean isVisible() {
            return mChip != null && mChipVisibilityState == VisibilityState.EXPANDED;
        }

        @Override
        public int updateVisibility(int availableWidth) {
            if (mChip == null) return 0;
            if (mChipVisibilityState == VisibilityState.HIDDEN) return 0;

            int neededAdditionalWidth = mMinExpandedWidth - mCollapsedWidth;
            if (availableWidth < neededAdditionalWidth) {
                mChipVisibilityState = VisibilityState.COLLAPSED;
                mChip.setAvailableWidth(mCollapsedWidth);
                // Consume all the width to prevent any smaller components from showing.
                return availableWidth;
            }

            mChipVisibilityState = VisibilityState.EXPANDED;
            mChip.setAvailableWidth(Math.min(mCollapsedWidth + availableWidth, mMaxExpandedWidth));
            return Math.max(0, mChip.measureWidth() - mCollapsedWidth);
        }

        @Override
        public int updateVisibilityWithAnimation(
                int availableWidth, Collection<Animator> animators) {
            return updateVisibility(availableWidth);
        }
    }

    private final ViewGroup mRootView;
    private final LocationBarEmbedder mLocationBarEmbedder;
    private final @Px int mCollapsedWidth;
    private final @Px int mMinExpandedWidth;
    private final @Px int mMaxExpandedWidth;
    private final ToolbarWidthConsumer mCollapsedToolbarWidthConsumer;
    private final ToolbarWidthConsumer mExpandedToolbarWidthConsumer;

    private @Nullable OmniboxChipCoordinator mChip;
    private @VisibilityState int mChipVisibilityState;
    private @Nullable ChipCallback mChipCallback;
    private boolean mOmniboxFocused;

    /**
     * Creates an instance of {@link OmniboxChipManager}.
     *
     * @param rootView The root {@link ViewGroup} that will house the chip view.
     * @param locationBarEmbedder The {@link LocationBarEmbedder} to notify of visibility changes.
     */
    public OmniboxChipManager(ViewGroup rootView, LocationBarEmbedder locationBarEmbedder) {
        mRootView = rootView;
        mLocationBarEmbedder = locationBarEmbedder;
        var context = mRootView.getContext();
        var res = context.getResources();
        mCollapsedWidth = AttrUtils.getDimensionPixelSize(context, R.attr.minInteractTargetSize);
        mMinExpandedWidth = res.getDimensionPixelSize(R.dimen.omnibox_chip_min_expanded_width);
        mMaxExpandedWidth = res.getDimensionPixelSize(R.dimen.omnibox_chip_max_expanded_width);
        mCollapsedToolbarWidthConsumer = new CollapsedToolbarWidthConsumer();
        mExpandedToolbarWidthConsumer = new ExpandedToolbarWidthConsumer();
    }

    public void destroy() {
        dismissChip();
    }

    /**
     * Places a chip in the Omnibox with the specified properties. If there is already a chip
     * placed, updates its properties.
     *
     * @param text The text to display when the chip is in its expanded state.
     * @param icon The icon drawable to display on the chip.
     * @param contentDesc The content description of the chip.
     * @param onClick A runnable to execute when the chip is clicked.
     * @param callback A callback to get notified when the chip is hidden or shown.
     */
    public void placeChip(
            String text,
            Drawable icon,
            String contentDesc,
            Runnable onClick,
            ChipCallback callback) {
        var model =
                new PropertyModel.Builder(OmniboxChipProperties.ALL_KEYS)
                        .with(TEXT, text)
                        .with(ICON, icon)
                        .with(CONTENT_DESC, contentDesc)
                        .with(ON_CLICK, onClick)
                        .build();
        mChipCallback = callback;
        if (mChip == null) {
            mChip = new OmniboxChipCoordinator(mRootView, model);
        } else {
            mChip.updateChip(text, icon, contentDesc, onClick);
        }

        mRootView.setVisibility(getRootVisibility());
        mChipVisibilityState = VisibilityState.UNINITIALIZED;
        mLocationBarEmbedder.onWidthConsumerVisibilityChanged();
    }

    /** Dismisses the existing chip, if exists. */
    public void dismissChip() {
        if (mChip != null) {
            mChip.destroy();
            mChip = null;
            mChipCallback = null;
            mChipVisibilityState = VisibilityState.UNINITIALIZED;
            mRootView.setVisibility(View.GONE);
            mLocationBarEmbedder.onWidthConsumerVisibilityChanged();
        }
    }

    /** Returns whether the chip is placed. */
    public boolean isChipPlaced() {
        return mChip != null;
    }

    /**
     * Sets whether the omnibox is focused and the chip should be hidden.
     *
     * @param focused Whether the omnibox is focused.
     */
    public void setOmniboxFocused(boolean focused) {
        mOmniboxFocused = focused;
        mRootView.setVisibility(getRootVisibility());
    }

    /** Returns the {@link ToolbarWidthConsumer} for the collapsed (icon only) state. */
    public ToolbarWidthConsumer getCollapsedToolbarWidthConsumer() {
        return mCollapsedToolbarWidthConsumer;
    }

    /** Returns the {@link ToolbarWidthConsumer} for the expanded (icon + text) state. */
    public ToolbarWidthConsumer getExpandedToolbarWidthConsumer() {
        return mExpandedToolbarWidthConsumer;
    }

    @Px
    int getCollapsedWidthForTesting() {
        return mCollapsedWidth;
    }

    @Px
    int getMinExpandedWidthForTesting() {
        return mMinExpandedWidth;
    }

    private int getRootVisibility() {
        if (!isChipPlaced()) return View.GONE;

        return mOmniboxFocused ? View.INVISIBLE : View.VISIBLE;
    }
}
