// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.EMPTY_SIDE_UI_SPECS;

import android.animation.TimeInterpolator;
import android.app.Activity;
import android.content.res.Configuration;
import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.util.ArrayMap;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewParent;
import android.view.ViewStub;

import androidx.annotation.Px;
import androidx.core.view.animation.PathInterpolatorCompat;
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

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Implementation of {@link SideUiCoordinator}. */
@NullMarked
final class SideUiCoordinatorImpl implements SideUiCoordinator, ConfigurationChangedObserver {

    private static final long TRANSITION_DURATION_MS = 350L;

    private final Activity mParentActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    private final ViewGroup mAnchorContainerParent;

    private final ObserverList<SideUiObserver> mSideUiObservers = new ObserverList<>();

    private final NonNullObservableSupplier<Integer> mTopMarginSupplier;
    private final Callback<Integer> mTopMarginObserver;

    /** Maps {@link AnchorSide} to {@link ViewGroup} where {@link SideUiContainer} is attached. */
    private final Map<Integer, ViewGroup> mAnchorContainers = new ArrayMap<>();

    /** List of registered {@link SideUiContainer} objects. */
    private final List<SideUiContainer> mSideUiContainers = new ArrayList<>();

    /**
     * Constructor for a {@link SideUiCoordinatorImpl}.
     *
     * @param parentActivity The {@link Activity} containing all Side UIs.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for {@code
     *     parentActivity}.
     * @param anchorContainerParent The {@link ViewGroup} that is the parent for the side UI
     *     containers.
     * @param leftAnchorContainerStub The {@link ViewStub} for the left-anchored container.
     * @param rightAnchorContainerStub The {@link ViewStub} for the right-anchored container.
     * @param topMarginSupplier The supplier for the Side UI's top margin.
     */
    /* package */ SideUiCoordinatorImpl(
            Activity parentActivity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ViewGroup anchorContainerParent,
            ViewStub leftAnchorContainerStub,
            ViewStub rightAnchorContainerStub,
            NonNullObservableSupplier<Integer> topMarginSupplier) {
        mParentActivity = parentActivity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mAnchorContainerParent = anchorContainerParent;

        // TODO(crbug.com/485309827): Account for the height of Side UI. Specifically, show beneath
        //  the tab strip when it is visible.
        ViewGroup leftAnchorContainer = (ViewGroup) leftAnchorContainerStub.inflate();
        ViewGroup rightAnchorContainer = (ViewGroup) rightAnchorContainerStub.inflate();
        assert mAnchorContainerParent == leftAnchorContainer.getParent();
        assert mAnchorContainerParent == rightAnchorContainer.getParent();
        mAnchorContainers.put(AnchorSide.LEFT, leftAnchorContainer);
        mAnchorContainers.put(AnchorSide.RIGHT, rightAnchorContainer);

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
        assert sideUiContainer.getAnchorSide() == AnchorSide.LEFT
                        || sideUiContainer.getAnchorSide() == AnchorSide.RIGHT
                : "Only LEFT/RIGHT anchor side are supported for now";
        if (hasConflictingAnchorSides(sideUiContainer)) {
            throw new IllegalArgumentException(
                    String.format(
                            Locale.US,
                            "The container [id: %d, anchor-side: %d] has a conflict with existing"
                                    + " ones.",
                            sideUiContainer.getSideUiId(),
                            sideUiContainer.getAnchorSide()));
        }
        mSideUiContainers.add(sideUiContainer);

        // Keep the containers in descending order of the priority.
        mSideUiContainers.sort((c1, c2) -> c1.getSideUiId() - c2.getSideUiId());
    }

    @Override
    public void unregisterSideUiContainer(SideUiContainer sideUiContainer) {
        assert mSideUiContainers.size() == 1 && mSideUiContainers.get(0) == sideUiContainer
                : "Unregistering unknown SideUiContainer.";
        mSideUiContainers.remove(sideUiContainer);
    }

    @Override
    public void requestUpdateContainer(
            SideUiContainerProperties properties, boolean suppressAnimations) {
        @Px int windowWidth = getWindowWidth();
        @Px int minWebContentsWidth = ViewUtils.dpToPx(mParentActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        requestUpdateContainerInternal(
                properties,
                getCurrentSideUiSpecs(),
                windowWidth,
                minWebContentsWidth,
                suppressAnimations);
    }

    @Override
    public void destroy() {
        mSideUiContainers.clear();
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
        mSideUiObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(SideUiObserver observer) {
        mSideUiObservers.removeObserver(observer);
    }

    @Override
    public SideUiSpecs getCurrentSideUiSpecs() {
        // Note: When a View's visibility is changed to View.GONE, it won't be laid out so Android
        // won't update the View's internal states tracking its size. This means View.getWidth() can
        // return a stale value when the visibility is View.GONE.
        //
        // Therefore, we need to explicitly check if the visibility is View.GONE, and if so, return
        // 0.
        Map<Integer, Integer> anchorContainerWidths = new ArrayMap<>();
        for (Map.Entry<Integer, ViewGroup> entry : mAnchorContainers.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            ViewGroup anchorContainer = entry.getValue();
            @Px
            int anchorContainerWidth =
                    anchorContainer.getVisibility() == View.GONE ? 0 : anchorContainer.getWidth();
            anchorContainerWidths.put(anchorSide, anchorContainerWidth);
        }

        return new SideUiSpecs(anchorContainerWidths);
    }

    @Override
    public boolean isSideUiShowing(@SideUiId int sideUiId) {
        for (var sideUiContainer : mSideUiContainers) {
            if (sideUiContainer.getSideUiId() != sideUiId) continue;

            ViewGroup anchorContainer = mAnchorContainers.get(sideUiContainer.getAnchorSide());
            if (anchorContainer == null) return false;

            int width =
                    anchorContainer.getVisibility() == View.GONE ? 0 : anchorContainer.getWidth();
            return width > 0;
        }
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of SideUiStateProvider Implementation                                    //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // ConfigurationChangedObserver Implementation
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mSideUiContainers.isEmpty()) {
            return;
        }

        assert mSideUiContainers.size() == 1;
        var sideUiContainer = mSideUiContainers.get(0);

        // 1. Get the current SideUiSpecs.
        SideUiSpecs currentSideUiSpecs = getCurrentSideUiSpecs();
        @AnchorSide int currentAnchorSide = sideUiContainer.getAnchorSide();
        @Px
        int currentSideUiWidth =
                currentAnchorSide == AnchorSide.LEFT
                        ? currentSideUiSpecs.getWidth(AnchorSide.LEFT)
                        : currentSideUiSpecs.getWidth(AnchorSide.RIGHT);

        // 2. Check if we need to close/re-open side UI.
        //
        // Currently there is only one SideUiContainer, so we only need to check if there is enough
        // space for that container.
        //
        // When there are multiple SideUiContainers, we need to check if each container needs to
        // be closed/re-opened/resized, based on their priority.
        //
        // TODO(crbug.com/478338737): Update to account for multiple side containers.
        // TODO(crbug.com/515164601): Use SideUiContainer#determineContainerWidth() to decide
        // whether each SideUi should be shown.
        @Px int windowWidth = getWindowWidth();
        @Px int minWebContentsWidth = ViewUtils.dpToPx(mParentActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        @Px int minSideUiWidth = ViewUtils.dpToPx(mParentActivity, sideUiContainer.getMinWidthDp());
        boolean canShowSideUi = (windowWidth - minWebContentsWidth - minSideUiWidth >= 0);

        // 2.1. Check if we need to close side UI.
        if (currentSideUiWidth != 0 && !canShowSideUi) {
            // Don't simply close the side UI by calling requestUpdateContainerInternal() with a
            // zero width. SideUiContainer may need to save states so that it can restore its UI
            // when the window becomes large enough again.
            //
            // So we should notify SideUiContainer, which should then call requestUpdateContainer().
            sideUiContainer.onWindowResized(/* canShowSideUi= */ false);
            return;
        }

        // 2.2 Check if we need to re-open side UI.
        if (currentSideUiWidth == 0 && canShowSideUi) {
            // Similarly, we shouldn't call requestUpdateContainerInternal() here.
            // SideUiContainer needs to check whether there actually exists side UI to restore,
            // based on its internal states.
            sideUiContainer.onWindowResized(/* canShowSideUi= */ true);
            return;
        }

        // 3. At this point, we don't need to close or re-open side UI, so we'll just resize side
        // UI by requesting a UI update with the current specs. This will re-calculate the
        // SideUiSpecs, and if the re-calculated SideUiSpecs is different, the UI will be updated.
        requestUpdateContainerInternal(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(), currentAnchorSide, currentSideUiWidth),
                currentSideUiSpecs,
                windowWidth,
                minWebContentsWidth,
                /* suppressAnimations= */ true);
    }

    private boolean hasConflictingAnchorSides(SideUiContainer sideUiContainer) {
        List<Integer> allocatedAnchorSide = new ArrayList<>();
        @SideUiId int id = sideUiContainer.getSideUiId();

        for (SideUiContainer container : mSideUiContainers) {
            // Existing containers should have no conflict between each other.
            assert !allocatedAnchorSide.contains(container.getAnchorSide());
            allocatedAnchorSide.add(container.getAnchorSide());
            if (id == container.getSideUiId()) return true;
        }
        return allocatedAnchorSide.contains(sideUiContainer.getAnchorSide());
    }

    private void requestUpdateContainerInternal(
            SideUiContainerProperties requestedSideUiProperties,
            SideUiSpecs currentSideUiSpecs,
            @Px int windowWidth,
            @Px int minWebContentsWidth,
            boolean suppressAnimations) {
        // 1. Verify the request is valid.
        assert mSideUiContainers.size() == 1;
        var sideUiContainer = mSideUiContainers.get(0);
        assert requestedSideUiProperties.mWidth >= 0
                : "Requested a negative width for SideUiContainer.";

        // 2. Check if animations should be disabled entirely.
        suppressAnimations =
                suppressAnimations
                        || ChromeFeatureList.sEnableAndroidSidePanelDisableAnimations.getValue();

        // 3. Determine the upcoming SideUiSpecs.
        // Currently we only have one side UI container, so "availableWidth" is "windowWidth -
        // minWebContentsWidth".
        // TODO(crbug.com/478338737): Update to account for multiple side containers.
        @Px int availableWidth = windowWidth - minWebContentsWidth;
        @Px
        int finalSideUiWidth =
                sideUiContainer.determineContainerWidth(
                        requestedSideUiProperties.mWidth, availableWidth, windowWidth);
        var newSideUiSpecs =
                new SideUiSpecs(
                        requestedSideUiProperties.mAnchorSide == AnchorSide.LEFT
                                ? finalSideUiWidth
                                : 0,
                        requestedSideUiProperties.mAnchorSide == AnchorSide.RIGHT
                                ? finalSideUiWidth
                                : 0);

        // 4. Commit the new SideUiSpecs if it's different from the current SideUiSpecs.
        if (!newSideUiSpecs.equals(currentSideUiSpecs)) {
            // If animating, notify observers of the new SideUiSpecs and gather their Transitions
            // into a TransitionSet.
            @Nullable TransitionSet transitionSet =
                    suppressAnimations
                            ? null
                            : collectTransitions(
                                    newSideUiSpecs, requestedSideUiProperties.mAnchorSide);
            commitNewSideUiSpecs(
                    newSideUiSpecs, transitionSet, requestedSideUiProperties.mAnchorSide);
        }
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
        assert mSideUiContainers.size() == 1;

        // Rather than use a standard Android or Material interpolator, we instead match the desktop
        // impl's curve found at chrome/browser/ui/views/animations/side_panel_animations.cc.
        TimeInterpolator interpolator = PathInterpolatorCompat.create(0.45f, 0f, 0.12f, 1f);
        TransitionSet transitionSet =
                new TransitionSet()
                        .setDuration(TRANSITION_DURATION_MS)
                        .setOrdering(TransitionSet.ORDERING_TOGETHER)
                        .setInterpolator(interpolator);

        // Add transitions for the side UI container.
        // TODO(crbug.com/478338737): Update to account for multiple side containers.
        ViewGroup anchorContainer = assumeNonNull(mAnchorContainers.get(anchorSide));
        transitionSet.addTransition(
                SideUiContainerTransition.createContainerTransition(
                        anchorContainer, anchorSide, sideUiSpecs.getWidth(anchorSide)));
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
        // TODO(crbug.com/478338737): Update to account for multiple SideUiContainers.
        assert sideUiSpecs.getWidth(AnchorSide.LEFT)
                                == EMPTY_SIDE_UI_SPECS.getWidth(AnchorSide.LEFT)
                        || sideUiSpecs.getWidth(AnchorSide.RIGHT)
                                == EMPTY_SIDE_UI_SPECS.getWidth(AnchorSide.RIGHT)
                : "Only one SideUiContainer is supported for now, so SideUiSpecs can't have"
                        + " specs for more than one container";

        // End any existing transitions still in progress.
        TransitionManager.endTransitions(getRootView());

        // Update the side containers, with Transitions if available.
        if (transitionSet != null) {
            commitNewSpecsForAnimatedResize(transitionSet, sideUiSpecs, anchorSide);
        } else {
            commitNewSpecsForStaticResize(sideUiSpecs, anchorSide);
        }
    }

    private void commitNewSpecsForAnimatedResize(
            TransitionSet transitionSet, SideUiSpecs sideUiSpecs, @AnchorSide int anchorSide) {
        assert mSideUiContainers.size() == 1;
        var sideUiContainer = mSideUiContainers.get(0);

        // TODO(crbug.com/478338737): Update to account for multiple SideUiContainers.
        @Px
        int sideUiWidth =
                anchorSide == AnchorSide.LEFT
                        ? sideUiSpecs.getWidth(AnchorSide.LEFT)
                        : sideUiSpecs.getWidth(AnchorSide.RIGHT);

        // Ensure side UI container is attached and, if showing, starts offscreen with the
        // side UI width. If hiding, i.e. side UI width is 0, then setWidth() should be
        // delayed until after the Transition is finished.
        attachSideUiContainerView(sideUiContainer, anchorSide);
        if (sideUiWidth != 0) {
            sideUiContainer.setWidth(sideUiWidth);
        }

        // TODO(crbug.com/478338737): Update to account for multiple side containers, and move
        //  into collectTransitions() if possible.
        transitionSet.addListener(
                new TransitionListenerAdapter() {
                    @Override
                    public void onTransitionEnd(Transition transition) {
                        // Detach and close the container after the transition is complete.
                        if (sideUiWidth == 0) {
                            detachSideUiContainerView(sideUiContainer);
                            sideUiContainer.setWidth(0);
                        }
                        sideUiContainer.onContainerResized(sideUiWidth);
                        for (SideUiObserver observer : mSideUiObservers) {
                            observer.onTransitionEnded(sideUiSpecs);
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
        ViewGroup anchorContainer = assumeNonNull(mAnchorContainers.get(anchorSide));
        SideUiContainerTransition.triggerContainerTransition(
                anchorContainer, anchorContainer.getWidth(), anchorSide, sideUiWidth);
        for (SideUiObserver observer : mSideUiObservers) {
            observer.onTransitionBegun(sideUiSpecs);
        }
    }

    private void commitNewSpecsForStaticResize(
            SideUiSpecs sideUiSpecs, @AnchorSide int anchorSide) {
        assert mSideUiContainers.size() == 1;
        var sideUiContainer = mSideUiContainers.get(0);

        // TODO(crbug.com/478338737): Update to account for multiple SideUiContainers.
        @Px
        int sideUiWidth =
                anchorSide == AnchorSide.LEFT
                        ? sideUiSpecs.getWidth(AnchorSide.LEFT)
                        : sideUiSpecs.getWidth(AnchorSide.RIGHT);

        // Reset the side UI containers to clear any leftover state from previous Transitions.
        SideUiContainerTransition.resetContainer(
                assumeNonNull(mAnchorContainers.get(AnchorSide.LEFT)));
        SideUiContainerTransition.resetContainer(
                assumeNonNull(mAnchorContainers.get(AnchorSide.RIGHT)));

        if (sideUiWidth != 0) {
            attachSideUiContainerView(sideUiContainer, anchorSide);
        } else {
            detachSideUiContainerView(sideUiContainer);
        }
        sideUiContainer.setWidth(sideUiWidth);
        sideUiContainer.onContainerResized(sideUiWidth);

        notifySideUiSpecsChanged(sideUiSpecs);
    }

    /**
     * Attach the provided {@link SideUiContainer}'s {@link View} to its appropriate ViewGroup
     * determined by the {@link AnchorSide}.
     *
     * @param sideUiContainer The {@link SideUiContainer} whose view is to be attached.
     * @param anchorSide The requested {@link AnchorSide}.
     */
    private void attachSideUiContainerView(
            SideUiContainer sideUiContainer, @AnchorSide int anchorSide) {
        ViewGroup anchorContainer = mAnchorContainers.get(anchorSide);
        assert anchorContainer != null : "AnchorContainer is not available on the request side.";
        attachSideUiContainerView(sideUiContainer, anchorContainer);
    }

    /**
     * Attach the provided {@link SideUiContainer}'s {@link View} to the target parent {@link
     * ViewGroup}. Detaches from the other parent {@link ViewGroup} if needed. Ensures the anchor
     * container's visibility is VISIBLE.
     *
     * <p>No-op if the View was already attached.
     *
     * @param sideUiContainer The {@link SideUiContainer} whose view is to be attached.
     * @param targetParent The target {@link ViewGroup} to attach to.
     */
    private void attachSideUiContainerView(
            SideUiContainer sideUiContainer, ViewGroup targetParent) {
        View sideUiContainerView = sideUiContainer.getView();
        ViewParent currentParent = sideUiContainerView.getParent();

        // No-op if already attached.
        if (currentParent == targetParent) return;

        // Detach from the current parent, if any.
        detachSideUiContainerView(sideUiContainer);

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
     * @param sideUiContainer The {@link SideUiContainer} whose view is to be detached.
     */
    private void detachSideUiContainerView(SideUiContainer sideUiContainer) {
        View sideUiContainerView = sideUiContainer.getView();
        ViewParent currentParent = sideUiContainerView.getParent();

        // No-op if already detached.
        if (currentParent == null) {
            return;
        }

        var anchorContainer = mAnchorContainers.get(sideUiContainer.getAnchorSide());
        assert anchorContainer != null && anchorContainer == currentParent
                : "SideUiContainer was attached to an unknown group.";

        anchorContainer.removeView(sideUiContainerView);
        assert anchorContainer.getChildCount() == 0;
        anchorContainer.setVisibility(View.GONE);
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
        for (ViewGroup anchorContainer : mAnchorContainers.values()) {
            MarginLayoutParams layoutParams =
                    ((MarginLayoutParams) anchorContainer.getLayoutParams());
            layoutParams.topMargin = tabStripBottomPx;
            anchorContainer.setLayoutParams(layoutParams);
        }
    }

    private @Px int getWindowWidth() {
        return WindowMetricsCalculator.getOrCreate()
                .computeCurrentWindowMetrics(mParentActivity)
                .getBounds()
                .width();
    }

    // Test Support

    @Nullable SideUiContainer getSideUiContainerForTesting(@SideUiId int id) {
        for (SideUiContainer container : mSideUiContainers) {
            if (container.getSideUiId() == id) return container;
        }
        return null;
    }
}
