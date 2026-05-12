// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static android.view.View.MeasureSpec.EXACTLY;
import static android.view.View.MeasureSpec.makeMeasureSpec;

import static org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.EMPTY_SIDE_UI_SPECS;

import android.app.Activity;
import android.content.res.Configuration;
import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewParent;
import android.view.ViewStub;

import androidx.annotation.Px;
import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.Interpolators;

/** Implementation of {@link SideUiCoordinator}. */
@NullMarked
final class SideUiCoordinatorImpl implements SideUiCoordinator, ConfigurationChangedObserver {

    private static final long TRANSITION_DURATION_MS = 350L;

    private final Activity mParentActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    private final ViewGroup mAnchorContainerParent;
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
     * @param parentActivity The {@link Activity} containing all Side UIs.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for {@code
     *     parentActivity}.
     * @param anchorContainerParent The {@link ViewGroup} that is the parent for the side UI
     *     containers.
     * @param startAnchorContainerStub The {@link ViewStub} for the start-anchored container.
     * @param endAnchorContainerStub The {@link ViewStub} for the end-anchored container.
     * @param topMarginSupplier The supplier for the Side UI's top margin.
     */
    /* package */ SideUiCoordinatorImpl(
            Activity parentActivity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ViewGroup anchorContainerParent,
            ViewStub startAnchorContainerStub,
            ViewStub endAnchorContainerStub,
            NonNullObservableSupplier<Integer> topMarginSupplier) {
        mParentActivity = parentActivity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mAnchorContainerParent = anchorContainerParent;

        // TODO(crbug.com/485309827): Account for the height of Side UI. Specifically, show beneath
        //  the tab strip when it is visible.
        mStartAnchorContainer = (ViewGroup) startAnchorContainerStub.inflate();
        mEndAnchorContainer = (ViewGroup) endAnchorContainerStub.inflate();
        assert mAnchorContainerParent == mStartAnchorContainer.getParent();
        assert mAnchorContainerParent == mEndAnchorContainer.getParent();

        mTopMarginObserver = this::onTopMarginChanged;
        mTopMarginSupplier = topMarginSupplier;
        mTopMarginSupplier.addSyncObserver(mTopMarginObserver);

        mActivityLifecycleDispatcher.register(this);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start of SideUiCoordinator Implementation                                    //
    ///////////////////////////////////////////////////////////////////////////////////////////////

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
    public void requestUpdateContainer(
            SideUiContainerProperties properties, boolean suppressAnimations) {
        requestUpdateContainerInternal(properties, suppressAnimations);
    }

    @Override
    public void destroy() {
        if (mSideUiContainer != null) {
            unregisterSideUiContainer(mSideUiContainer);
        }
        mTopMarginSupplier.removeObserver(mTopMarginObserver);
        mActivityLifecycleDispatcher.unregister(this);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of SideUiCoordinator Implementation                                      //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start of SideUiStateProvider Implementation                                  //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    @Override
    public void addObserver(SideUiObserver observer) {
        if (mSideUiObservers.addObserver(observer)) {
            observer.onSideUiSpecsChanged(measureSideUiSpecs());
        }
    }

    @Override
    public void removeObserver(SideUiObserver observer) {
        if (mSideUiObservers.removeObserver(observer)) {
            observer.onSideUiSpecsChanged(SideUiSpecs.EMPTY_SIDE_UI_SPECS);
        }
    }

    @Override
    public SideUiSpecs measureSideUiSpecs() {
        View sideUiParent = (View) mStartAnchorContainer.getParent();
        assert sideUiParent == mEndAnchorContainer.getParent()
                : "Anchor containers should have the same parent.";

        int sideUiParentHeight = sideUiParent != null ? sideUiParent.getMeasuredHeight() : 0;
        int sideUiHeight = sideUiParentHeight - mSideUiTopMargin;
        int sideUiHeightSpec =
                sideUiHeight > 0
                        ? makeMeasureSpec(sideUiHeight, EXACTLY)
                        : makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int sideUiWidthSpec = makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        mStartAnchorContainer.measure(sideUiWidthSpec, sideUiHeightSpec);
        mEndAnchorContainer.measure(sideUiWidthSpec, sideUiHeightSpec);

        return new SideUiSpecs(
                mStartAnchorContainer.getMeasuredWidth(), mEndAnchorContainer.getMeasuredWidth());
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of SideUiStateProvider Implementation                                    //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // ConfigurationChangedObserver Implementation
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mSideUiContainer == null) {
            return;
        }

        SideUiSpecs currentSideUiSpecs = getCurrentSideUiSpecs();
        @AnchorSide int currentAnchorSide = mSideUiContainer.getAnchorSide();
        @Px
        int currentSideUiWidth =
                currentAnchorSide == AnchorSide.START
                        ? currentSideUiSpecs.mStartContainerWidth
                        : currentSideUiSpecs.mEndContainerWidth;

        // Requesting a UI update with the current specs will re-calculate the SideUiSpecs, and
        // if the re-calculated SideUiSpecs is different, the UI will be updated.
        requestUpdateContainerInternal(
                new SideUiContainerProperties(currentAnchorSide, currentSideUiWidth),
                /* suppressAnimations= */ true);
    }

    private void requestUpdateContainerInternal(
            SideUiContainerProperties properties, boolean suppressAnimations) {
        // 1. Verify the request is valid.
        assert mSideUiContainer != null
                : "#requestUpdateContainer called with null SideUiContainer.";
        assert properties.mWidth >= 0 : "SideUiContainer unexpectedly requested a negative width.";

        // 2. Check if animations should be disabled entirely.
        suppressAnimations =
                suppressAnimations
                        || ChromeFeatureList.sEnableAndroidSidePanelDisableAnimations.getValue();

        // 3. Get the current SideUiSpecs.
        var currentSideUiSpecs = getCurrentSideUiSpecs();

        // 4. Determine the upcoming SideUiSpecs.
        // Currently we only have one side UI container, so "availableWidth" is "windowWidth -
        // minWebContentsWidth".
        // TODO(crbug.com/478338737): Update to account for multiple side containers.
        @Px int windowWidth = getWindowWidth();
        @Px int minWebContentsWidth = ViewUtils.dpToPx(mParentActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        @Px int availableWidth = windowWidth - minWebContentsWidth;
        @Px
        int finalSideUiWidth =
                mSideUiContainer.determineContainerWidth(
                        properties.mWidth, availableWidth, windowWidth);
        var newSideUiSpecs =
                new SideUiSpecs(
                        properties.mAnchorSide == AnchorSide.START ? finalSideUiWidth : 0,
                        properties.mAnchorSide == AnchorSide.END ? finalSideUiWidth : 0);

        // 5. Commit the new SideUiSpecs if it's different from the current SideUiSpecs.
        if (!newSideUiSpecs.equals(currentSideUiSpecs)) {
            // If animating, notify observers of the new SideUiSpecs and gather their Transitions
            // into a TransitionSet.
            @Nullable TransitionSet transitionSet =
                    suppressAnimations
                            ? null
                            : collectTransitions(newSideUiSpecs, properties.mAnchorSide);
            commitNewSideUiSpecs(newSideUiSpecs, transitionSet, properties.mAnchorSide);
        }
    }

    private SideUiSpecs getCurrentSideUiSpecs() {
        // Note: When a View's visibility is changed to View.GONE, it won't be laid out so Android
        // won't update the View's internal states tracking its size. This means View.getWidth() can
        // return a stale value when the visibility is View.GONE.
        //
        // Therefore, we need to explicitly check if the visibility is View.GONE, and if so, return
        // 0.
        @Px
        int startAnchorContainerWidth =
                mStartAnchorContainer.getVisibility() == View.GONE
                        ? 0
                        : mStartAnchorContainer.getWidth();
        @Px
        int endAnchorContainerWidth =
                mEndAnchorContainer.getVisibility() == View.GONE
                        ? 0
                        : mEndAnchorContainer.getWidth();

        return new SideUiSpecs(startAnchorContainerWidth, endAnchorContainerWidth);
    }

    private ViewGroup getRootView() {
        return (ViewGroup) mAnchorContainerParent.getRootView();
    }

    /**
     * Collects Transitions from the SideUiObservers to animate an update to the containers, and
     * returns a TransitionSet that plays all the Transitions together.
     *
     * @param sideUiSpecs The new SideUiSpecs representing the state for the end of the Transition.
     * @param anchorSide The side to which the side container is anchored.
     */
    // TODO(crbug.com/510059861): Add tests for transition animations.
    private TransitionSet collectTransitions(SideUiSpecs sideUiSpecs, @AnchorSide int anchorSide) {
        assert mSideUiContainer != null;

        TransitionSet transitionSet =
                new TransitionSet()
                        .setDuration(TRANSITION_DURATION_MS)
                        .setOrdering(TransitionSet.ORDERING_TOGETHER)
                        .setInterpolator(Interpolators.STANDARD_ACCELERATE);

        // Add transitions for the side UI container.
        // TODO(crbug.com/478338737): Update to account for multiple side containers.
        if (anchorSide == AnchorSide.START) {
            transitionSet.addTransition(
                    SideUiContainerTransition.createContainerTransition(
                            mStartAnchorContainer,
                            AnchorSide.START,
                            sideUiSpecs.mStartContainerWidth));
        } else {
            transitionSet.addTransition(
                    SideUiContainerTransition.createContainerTransition(
                            mEndAnchorContainer, AnchorSide.END, sideUiSpecs.mEndContainerWidth));
        }

        for (SideUiObserver observer : mSideUiObservers) {
            @Nullable Transition observerTransition = observer.onPreSideUiSpecsChange(sideUiSpecs);
            if (observerTransition != null) {
                transitionSet.addTransition(observerTransition);
            }
        }

        return transitionSet;
    }

    /**
     * Commits the newly calculated {@link SideUiSpecs} for {@link SideUiContainer}s.
     *
     * <p>This method will perform static resizing or animated resizing, depending on the presence
     * of the given {@code transitionSet}.
     *
     * @param sideUiSpecs The new {@link SideUiSpecs}.
     * @param transitionSet The {@link TransitionSet} directing the animation for the update. If
     *     null, then no animation is happening for the update.
     * @param anchorSide The requesting container's desired {@link AnchorSide}.
     */
    private void commitNewSideUiSpecs(
            SideUiSpecs sideUiSpecs,
            @Nullable TransitionSet transitionSet,
            @AnchorSide int anchorSide) {
        assert mSideUiContainer != null;

        // TODO(crbug.com/478338737): Update to account for multiple SideUiContainers.
        assert sideUiSpecs.mStartContainerWidth == EMPTY_SIDE_UI_SPECS.mStartContainerWidth
                        || sideUiSpecs.mEndContainerWidth == EMPTY_SIDE_UI_SPECS.mEndContainerWidth
                : "Only one SideUiContainer is supported for now, so SideUiSpecs can't have"
                        + " specs for more than one container";

        @Px
        int sideUiWidth =
                anchorSide == AnchorSide.START
                        ? sideUiSpecs.mStartContainerWidth
                        : sideUiSpecs.mEndContainerWidth;

        // End any existing transitions still in progress.
        TransitionManager.endTransitions(getRootView());

        // Update the side containers, with Transitions if available.
        if (transitionSet != null) {
            // Ensure side UI container is attached and, if showing, starts offscreen with the
            // side UI width. If hiding, i.e. side UI width is 0, then setWidth() should be
            // delayed until after the Transition is finished.
            attachSideUiContainerView(mSideUiContainer.getView(), anchorSide);
            if (sideUiWidth != 0) {
                mSideUiContainer.setWidth(sideUiWidth);
            }

            // TODO(crbug.com/478338737): Update to account for multiple side containers, and move
            //  into collectTransitions() if possible.
            transitionSet.addListener(
                    new TransitionListenerAdapter() {
                        @Override
                        public void onTransitionEnd(Transition transition) {
                            // Detach and close the container after the transition is complete.
                            if (sideUiWidth == 0) {
                                assert mSideUiContainer != null;
                                detachSideUiContainerView(mSideUiContainer.getView());
                                mSideUiContainer.setWidth(0);
                            }
                        }
                    });

            // Trigger a synchronous measure and layout pass on the container to ensure that the
            // starting snapshot for the Transition is updated and accurate. If this is not done,
            // the side panel can have visual bugs where it animates from an incorrect starting
            // point, especially if the window has been resized recently. Updating View attributes,
            // like setting a View's translation, is not enough alone.
            ViewUtils.triggerSynchronousMeasureAndLayout(mAnchorContainerParent);
            TransitionManager.beginDelayedTransition(getRootView(), transitionSet);
            if (anchorSide == AnchorSide.START) {
                SideUiContainerTransition.triggerContainerTransition(
                        mStartAnchorContainer,
                        mStartAnchorContainer.getWidth(),
                        AnchorSide.START,
                        sideUiWidth);
            } else {
                SideUiContainerTransition.triggerContainerTransition(
                        mEndAnchorContainer,
                        mEndAnchorContainer.getWidth(),
                        AnchorSide.END,
                        sideUiWidth);
            }
        } else {
            // Reset the side UI containers to clear any leftover state from previous Transitions.
            SideUiContainerTransition.resetContainer(mStartAnchorContainer);
            SideUiContainerTransition.resetContainer(mEndAnchorContainer);

            if (sideUiWidth != 0) {
                attachSideUiContainerView(mSideUiContainer.getView(), anchorSide);
            } else {
                detachSideUiContainerView(mSideUiContainer.getView());
            }
            mSideUiContainer.setWidth(sideUiWidth);
        }

        // Observers should be notified immediately, regardless of whether a Transition animation
        // has been started. If a delayed Transition has begun, the observers must be notified for
        // their changes to be captured in the Transition end values - the actual animation will
        // only begin after all observers have been notified.
        notifySideUiSpecsChanged(sideUiSpecs);
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
     * Notifies each {@link SideUiObserver} of the new {@link SideUiSpecs} that represents the
     * resting UI state.
     */
    private void notifySideUiSpecsChanged(SideUiSpecs sideUiSpecs) {
        for (SideUiObserver observer : mSideUiObservers) {
            observer.onSideUiSpecsChanged(sideUiSpecs);
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

    private @Px int getWindowWidth() {
        return WindowMetricsCalculator.getOrCreate()
                .computeCurrentWindowMetrics(mParentActivity)
                .getBounds()
                .width();
    }

    // Test Support

    @Nullable SideUiContainer getSideUiContainerForTesting() {
        return mSideUiContainer;
    }
}
