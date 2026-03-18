// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.ViewStub;

import androidx.annotation.Px;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;

/** Implementation of {@link SideUiCoordinator}. */
@NullMarked
final class SideUiCoordinatorImpl implements SideUiCoordinator {

    private final ViewGroup mStartAnchorContainer;
    private final ViewGroup mEndAnchorContainer;

    // TODO(crbug.com/478338737): Update to account for multiple side containers.
    @Nullable private SideUiContainer mSideUiContainer;
    private final ObserverList<SideUiObserver> mSideUiObservers = new ObserverList<>();

    /**
     * Constructor for a {@link SideUiCoordinatorImpl}.
     *
     * @param startAnchorContainerStub The {@link ViewStub} for the start-anchored container.
     * @param endAnchorContainerStub The {@link ViewStub} for the end-anchored container.
     */
    /* package */ SideUiCoordinatorImpl(
            ViewStub startAnchorContainerStub, ViewStub endAnchorContainerStub) {
        // TODO(crbug.com/485309827): Account for the height of Side UI. Specifically, show beneath
        //  the tab strip when it is visible.
        mStartAnchorContainer = (ViewGroup) startAnchorContainerStub.inflate();
        mEndAnchorContainer = (ViewGroup) endAnchorContainerStub.inflate();
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
        if (mSideUiContainer != null) unregisterSideUiContainer(mSideUiContainer);
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
        mStartAnchorContainer.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        mEndAnchorContainer.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
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
            attachSideUiContainerView(
                    sideUiContainerView, mStartAnchorContainer, mEndAnchorContainer);
        } else if (anchorSide == AnchorSide.END) {
            attachSideUiContainerView(
                    sideUiContainerView, mEndAnchorContainer, mStartAnchorContainer);
        } else {
            assert false : "SideUiContainer requested an unknown AnchorSide.";
        }
    }

    /**
     * Attach the provided {@link SideUiContainer}'s {@link View} to the target parent {@link
     * ViewGroup}. Detaches from the other parent {@link ViewGroup} if needed. No-op if the View was
     * already attached. Asserts that the View was not attached to an unexpected ViewGroup.
     *
     * @param sideUiContainerView The {@link SideUiContainer}'s {@link View} to attach.
     * @param targetParent The target {@link ViewGroup} to attach to.
     * @param otherParent The other {@link ViewGroup} that the View may need to be detached from.
     */
    private void attachSideUiContainerView(
            View sideUiContainerView, ViewGroup targetParent, ViewGroup otherParent) {
        ViewParent currentParent = sideUiContainerView.getParent();

        // No-op if already attached.
        if (currentParent == targetParent) return;

        // Attempt to remove from the other parent if switching anchor sides.
        if (currentParent == otherParent) {
            otherParent.removeView(sideUiContainerView);
        }

        // Re-grab the parent as it may have changed. Assert that we successfully detached.
        assert sideUiContainerView.getParent() == null
                : "SideUiContainer was attached to an unknown group.";

        // Attach to the target parent.
        targetParent.addView(sideUiContainerView);
    }

    /**
     * Detaches the provided {@link SideUiContainer}'s {@link View} from its parent {@link
     * ViewGroup}. No-op if it's already detached. Asserts that the View was not attached to an
     * unexpected ViewGroup.
     *
     * @param sideUiContainerView The {@link SideUiContainer}'s {@link View} to detach.
     */
    private void detachSideUiContainerView(View sideUiContainerView) {
        ViewParent currentParent = sideUiContainerView.getParent();

        // No-op if already detached.
        if (currentParent == null) return;

        if (currentParent == mStartAnchorContainer) {
            mStartAnchorContainer.removeView(sideUiContainerView);
        } else if (currentParent == mEndAnchorContainer) {
            mEndAnchorContainer.removeView(sideUiContainerView);
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

    // Test Support

    @Nullable SideUiContainer getSideUiContainerForTesting() {
        return mSideUiContainer;
    }
}
