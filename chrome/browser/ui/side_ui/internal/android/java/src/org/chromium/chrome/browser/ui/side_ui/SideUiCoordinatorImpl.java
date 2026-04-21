// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewParent;
import android.view.ViewStub;

import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;

/** Implementation of {@link SideUiCoordinator}. */
@NullMarked
final class SideUiCoordinatorImpl implements SideUiCoordinator {

    private final ViewGroup mStartAnchorContainer;
    private final ViewGroup mEndAnchorContainer;

    private final ObserverList<SideUiObserver> mSideUiObservers = new ObserverList<>();

    private final NonNullObservableSupplier<Integer> mTopMarginSupplier;
    private final Callback<Integer> mTopMarginObserver;

    // TODO(crbug.com/478338737): Update to account for multiple side containers.
    @Nullable private SideUiContainer mSideUiContainer;

    // TODO(crbug.com/478338737): Update to account for multiple side UIs with different top
    // margins.
    private @Px int mSideUiTopMargin;

    /**
     * Constructor for a {@link SideUiCoordinatorImpl}.
     *
     * @param startAnchorContainerStub The {@link ViewStub} for the start-anchored container.
     * @param endAnchorContainerStub The {@link ViewStub} for the end-anchored container.
     * @param topMarginSupplier The supplier for the Side UI's top margin.
     */
    /* package */ SideUiCoordinatorImpl(
            ViewStub startAnchorContainerStub,
            ViewStub endAnchorContainerStub,
            NonNullObservableSupplier<Integer> topMarginSupplier) {
        // TODO(crbug.com/485309827): Account for the height of Side UI. Specifically, show beneath
        //  the tab strip when it is visible.
        mStartAnchorContainer = (ViewGroup) startAnchorContainerStub.inflate();
        mEndAnchorContainer = (ViewGroup) endAnchorContainerStub.inflate();

        mTopMarginObserver = this::onTopMarginChanged;
        mTopMarginSupplier = topMarginSupplier;
        mTopMarginSupplier.addSyncObserver(mTopMarginObserver);
    }

    // SideUiCoordinator Implementation

    @Override
    public void registerSideUiContainer(SideUiContainer sideUiContainer) {
        assert mSideUiContainer == null : "Registering a SideUiContainer when already set.";
        mSideUiContainer = sideUiContainer;
    }

    @Override
    public void unregisterSideUiContainer(SideUiContainer sideUiContainer) {
        assert mSideUiContainer == sideUiContainer : "Unregistering unknown SideUiContainer.";
        mSideUiContainer = null;
    }

    @Override
    public void requestUpdateContainer(SideUiContainerProperties properties) {
        // 1. Verify the request is valid.
        assert mSideUiContainer != null
                : "#requestUpdateContainer called with null SideUiContainer.";
        assert properties.mWidth >= 0 : "SideUiContainer unexpectedly requested a negative width.";

        // 2. Determine containers' widths and the upcoming SideUiSpecs.
        // TODO(crbug.com/478306743): Loop through the registered SideUiContainers and call
        //  SideUiContainer#determineContainerWidth to determine all of the containers' accepted
        //  widths, rather than just implicitly accepting the requested width. Determine the
        //  upcoming SideUiSpecs based on the accepted widths.
        @Px int acceptedWidth = properties.mWidth;

        // 3. If animating, notify observers of the upcoming SideUiSpecs from the previous step.
        // TODO(crbug.com/491606333): Use the new SideUiSpecs calculated in the previous step to
        //  notify observers and collect animators. See the proposed event (#onPreSideUiSpecsChange)
        //  in SideUiObserver.java for more details.

        // 4. Commit the new SideUiSpecs.
        // TODO(crbug.com/478338737): Track accepted widths for all of the registered containers.
        commitNewSideUiSpecs(properties.mAnchorSide, acceptedWidth);
    }

    @Override
    public void destroy() {
        if (mSideUiContainer != null) {
            unregisterSideUiContainer(mSideUiContainer);
        }
        mTopMarginSupplier.removeObserver(mTopMarginObserver);
    }

    // SideUiStateProvider Implementation

    @Override
    public void addObserver(SideUiObserver observer) {
        if (mSideUiObservers.addObserver(observer)) {
            observer.onSideUiSpecsChanged(getCurrentSideUiSpecs());
        }
    }

    @Override
    public void removeObserver(SideUiObserver observer) {
        if (mSideUiObservers.removeObserver(observer)) {
            observer.onSideUiSpecsChanged(SideUiSpecs.EMPTY_SIDE_UI_SPECS);
        }
    }

    @Override
    public SideUiSpecs getCurrentSideUiSpecs() {
        // Infers by measuring the two parent containers.
        View sideUiParent = (View) mStartAnchorContainer.getParent();
        assert sideUiParent == mEndAnchorContainer.getParent()
                : "Anchor containers should have the same parent.";

        int sideUiParentHeight = sideUiParent != null ? sideUiParent.getHeight() : 0;
        int sideUiHeight = sideUiParentHeight - mSideUiTopMargin;
        int sideUiHeightSpec =
                sideUiHeight > 0
                        ? MeasureSpec.makeMeasureSpec(sideUiHeight, MeasureSpec.EXACTLY)
                        : MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int sideUiWidthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        mStartAnchorContainer.measure(sideUiWidthSpec, sideUiHeightSpec);
        mEndAnchorContainer.measure(sideUiWidthSpec, sideUiHeightSpec);

        return new SideUiSpecs(
                mStartAnchorContainer.getMeasuredWidth(), mEndAnchorContainer.getMeasuredWidth());
    }

    /**
     * Commits a {@link SideUiContainer}'s requested update. Currently, statically resizes and
     * attaches/detaches as necessary. As per the inline TODO, this will eventually support
     * dynamic/animated resizes, where we'll instead kick off animators to handle the resizes.
     *
     * @param anchorSide The requesting container's desired {@link AnchorSide}.
     * @param acceptedWidth The requesting container's accepted width in px.
     */
    @RequiresNonNull("mSideUiContainer")
    private void commitNewSideUiSpecs(@AnchorSide int anchorSide, @Px int acceptedWidth) {
        // TODO(crbug.com/491606333): Support dynamically updating/animating the changes. i.e.
        //  Rather than statically changing the SideUiContainers here, we would instead kick off
        //  animators to handle the width changes. We'd also defer 1) detaching the backing views
        //  and 2) notifying observers that the change is complete to #onAnimationEnd, as those
        //  depend on first reaching the resting state.
        View sideUiContainerView = mSideUiContainer.getView();
        if (acceptedWidth == 0) {
            detachSideUiContainerView(sideUiContainerView);
        } else {
            attachSideUiContainerView(sideUiContainerView, anchorSide);
        }
        mSideUiContainer.setWidth(acceptedWidth);

        notifySideUiSpecsChanged();
    }

    /**
     * Attach the provided {@link SideUiContainer}'s {@link View} to its appropriate ViewGroup
     * determined by the {@link AnchorSide}.
     *
     * @param sideUiContainerView The {@link SideUiContainer}'s {@link View} to attach.
     * @param anchorSide The requested {@link AnchorSide}.
     */
    private void attachSideUiContainerView(View sideUiContainerView, @AnchorSide int anchorSide) {
        if (anchorSide == AnchorSide.START) {
            attachSideUiContainerView(sideUiContainerView, mStartAnchorContainer);
        } else if (anchorSide == AnchorSide.END) {
            attachSideUiContainerView(sideUiContainerView, mEndAnchorContainer);
        } else {
            assert false : "SideUiContainer requested an unknown AnchorSide.";
        }
    }

    /**
     * Attach the provided {@link SideUiContainer}'s {@link View} to the target parent {@link
     * ViewGroup}. Detaches from the other parent {@link ViewGroup} if needed. Ensures the anchor
     * container's visibility is VISIBLE.
     *
     * <p>No-op if the View was already attached.
     *
     * @param sideUiContainerView The {@link SideUiContainer}'s {@link View} to attach.
     * @param targetParent The target {@link ViewGroup} to attach to.
     */
    private void attachSideUiContainerView(View sideUiContainerView, ViewGroup targetParent) {
        ViewParent currentParent = sideUiContainerView.getParent();

        // No-op if already attached.
        if (currentParent == targetParent) return;

        // Detach from the current parent, if any.
        detachSideUiContainerView(sideUiContainerView);

        // Attach to the target parent.
        targetParent.addView(sideUiContainerView);
        targetParent.setVisibility(View.VISIBLE);
    }

    /**
     * Detaches the provided {@link SideUiContainer}'s {@link View} from its parent {@link
     * ViewGroup}. Sets the anchor container's visibility to GONE if it no longer has any child
     * Views attached. Asserts that the View was not attached to an unexpected ViewGroup.
     *
     * <p>No-op if the View was already detached.
     *
     * @param sideUiContainerView The {@link SideUiContainer}'s {@link View} to detach.
     */
    private void detachSideUiContainerView(View sideUiContainerView) {
        ViewParent currentParent = sideUiContainerView.getParent();

        // No-op if already detached.
        if (currentParent == null) return;

        if (currentParent == mStartAnchorContainer) {
            mStartAnchorContainer.removeView(sideUiContainerView);
            assert mStartAnchorContainer.getChildCount() == 0;
            mStartAnchorContainer.setVisibility(View.GONE);
        } else if (currentParent == mEndAnchorContainer) {
            mEndAnchorContainer.removeView(sideUiContainerView);
            assert mEndAnchorContainer.getChildCount() == 0;
            mEndAnchorContainer.setVisibility(View.GONE);
        } else {
            assert false : "SideUiContainer was attached to an unknown group.";
        }
    }

    /**
     * Notifies each {@link SideUiObserver} of the new {@link SideUiSpecs}. Called after the
     * containers and their views have reached their resting state (and {@link
     * #getCurrentSideUiSpecs()} represents this resting state).
     */
    private void notifySideUiSpecsChanged() {
        SideUiSpecs newSideUiSpecs = getCurrentSideUiSpecs();
        for (SideUiObserver observer : mSideUiObservers) {
            observer.onSideUiSpecsChanged(newSideUiSpecs);
        }
    }

    /**
     * Called to respond to the tab strip location changing. The side UI anchor containers will
     * adjust their top margins accordingly.
     *
     * @param tabStripBottomPx The tab strip's bottom in relation to the top of the window in px.
     */
    private void onTopMarginChanged(@Px int tabStripBottomPx) {
        mSideUiTopMargin = tabStripBottomPx;

        MarginLayoutParams startLayoutParams =
                ((MarginLayoutParams) mStartAnchorContainer.getLayoutParams());
        startLayoutParams.topMargin = tabStripBottomPx;
        mStartAnchorContainer.setLayoutParams(startLayoutParams);

        MarginLayoutParams endLayoutParams =
                ((MarginLayoutParams) mEndAnchorContainer.getLayoutParams());
        endLayoutParams.topMargin = tabStripBottomPx;
        mEndAnchorContainer.setLayoutParams(endLayoutParams);
    }

    // Test Support

    @Nullable SideUiContainer getSideUiContainerForTesting() {
        return mSideUiContainer;
    }
}
