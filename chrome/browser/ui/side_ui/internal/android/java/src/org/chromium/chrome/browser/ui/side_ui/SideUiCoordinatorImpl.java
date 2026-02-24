// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.ViewStub;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

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
    public void requestUpdateContainer(SideUiContainerProperties properties) {
        assert mSideUiContainer != null
                : "#requestUpdateContainer called with null SideUiContainer.";

        int requestedWidth = properties.mWidth;
        assert requestedWidth >= 0 : "SideUiContainer unexpectedly requested a negative width.";

        // Determine the container widths.
        // TODO(crbug.com/478306743): Validate the request based on the available space,
        //  SideUiContainer#determineContainerWidth, etc. in this step instead of just accepting the
        //  request. When we support multiple SideUiContainers, we'll also need to loop through and
        //  determine all the final widths before pushing any of the individual changes (e.g. we'll
        //  want to aggregate all the animators before kicking off any individual one).
        @AnchorSide int anchorSide = properties.mAnchorSide;
        SideUiSpecs newSideUiSpecs = SideUiSpecs.EMPTY_SIDE_UI_SPECS;
        if (anchorSide == AnchorSide.START) {
            newSideUiSpecs = new SideUiSpecs(requestedWidth, /* endX= */ 0);
        } else if (anchorSide == AnchorSide.END) {
            newSideUiSpecs = new SideUiSpecs(/* startX= */ 0, requestedWidth);
        } else {
            assert false : "SideUiContainer requested an unknown AnchorSide.";
        }
        notifyObservers(newSideUiSpecs);

        // Push the new container widths. Attach/detach views if needed.
        View sideUiContainerView = mSideUiContainer.getView();
        if (requestedWidth == 0) {
            detachSideUiContainerView(sideUiContainerView);
        } else {
            attachSideUiContainerView(sideUiContainerView, properties.mAnchorSide);
        }
        mSideUiContainer.setWidth(properties.mWidth);
    }

    @Override
    public void destroy() {
        if (mSideUiContainer != null) unregisterSideUiContainer(mSideUiContainer);
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
     * Notifies each {@link SideUiObserver} of the new {@link SideUiSpecs}. Called when new {@link
     * SideUiSpecs} have been determined, but before these specs are actually used to statically
     * kick off a UI change.
     *
     * @param newSideUiSpecs The update {@link SideUiSpecs}.
     */
    private void notifyObservers(SideUiSpecs newSideUiSpecs) {
        for (SideUiObserver observer : mSideUiObservers) {
            observer.onSideUiSpecsChanged(newSideUiSpecs);
        }
    }

    /**
     * Returns the current {@link SideUiSpecs}. Infers based on measuring the two parent containers.
     * Should not be used in {@link #requestUpdateContainer} as that notifies Observers of the
     * updated {@link SideUiSpecs} before any UI changes are actually made.
     *
     * @return The current {@link SideUiSpecs}.
     */
    private SideUiSpecs getCurrentSideUiSpecs() {
        mStartAnchorContainer.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        mEndAnchorContainer.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        return new SideUiSpecs(
                mStartAnchorContainer.getMeasuredWidth(), mEndAnchorContainer.getMeasuredWidth());
    }

    // Test Support

    @Nullable SideUiContainer getSideUiContainerForTesting() {
        return mSideUiContainer;
    }
}
