// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.chromium.build.NullUtil.assumeNonNull;

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
        assert mSideUiContainers.contains(sideUiContainer)
                : "Unregistering unknown SideUiContainer.";
        mSideUiContainers.remove(sideUiContainer);
    }

    @Override
    public void requestUpdateContainer(
            SideUiContainerProperties properties, boolean suppressAnimations) {
        updateContainerWidths(properties, suppressAnimations);
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
        return getCurrentSideUiSpecsInternal();
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

        // TODO(crbug.com/478338737): Update to account for multiple side containers.
        if (mSideUiContainers.size() != 1) {
            return;
        }
        var sideUiContainer = mSideUiContainers.get(0);
        if (sideUiContainer.getSideUiId() != SideUiId.SIDE_PANEL) {
            return;
        }

        // 1. End any existing transitions still in progress. This needs to be done before checking
        // the current specs, since specs aren't fully updated until after all transitions have
        // finished.
        TransitionManager.endTransitions(getRootView());

        // 2. Get the current SideUiSpecs.
        SideUiSpecs currentSideUiSpecs = getCurrentSideUiSpecsInternal();
        @AnchorSide int currentAnchorSide = sideUiContainer.getAnchorSide();
        @Px
        int currentSideUiWidth =
                currentAnchorSide == AnchorSide.LEFT
                        ? currentSideUiSpecs.getWidth(AnchorSide.LEFT)
                        : currentSideUiSpecs.getWidth(AnchorSide.RIGHT);

        // 3. Check if we need to close/re-open SideUi.
        //
        // Note that when currentSideUiWidth is 0, we shouldn't use 0 as the requestedSideUiWidth
        // since doing so will cause the new SideUi width to be 0 and fail to re-open SideUi even if
        // the window has become wide enough.
        //
        // Therefore, when currentSideUiWidth is 0, we _request_ the SideUi's maximum width
        // (i.e., windowWidth - minWebContentsWidth) so that determineSideUiSpecs() can return a
        // non-zero width if the window is wide enough.
        @Px int windowWidth = getWindowWidth();
        @Px int minWebContentsWidth = ViewUtils.dpToPx(mParentActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        @Px
        int requestedSideUiWidth =
                currentSideUiWidth != 0 ? currentSideUiWidth : windowWidth - minWebContentsWidth;
        SideUiSpecs newSideUiSpecs =
                determineSideUiSpecs(
                        new SideUiContainerProperties(
                                sideUiContainer.getSideUiId(),
                                currentAnchorSide,
                                requestedSideUiWidth),
                        currentSideUiSpecs,
                        windowWidth,
                        minWebContentsWidth);
        boolean canShowSideUi = newSideUiSpecs.getWidth(currentAnchorSide) > 0;

        // 3.1. Check if we need to close side UI.
        if (currentSideUiWidth != 0 && !canShowSideUi) {
            // Don't simply close the side UI by calling commitNewSpecsForStaticResize() with a
            // zero width. SideUiContainer may need to save states so that it can restore its UI
            // when the window becomes large enough again.
            //
            // So we should notify SideUiContainer, which should then call requestUpdateContainer().
            sideUiContainer.onWindowResized(/* canShowSideUi= */ false);
            return;
        }

        // 3.2 Check if we need to re-open side UI.
        if (currentSideUiWidth == 0 && canShowSideUi) {
            // Similarly, we shouldn't call commitNewSpecsForStaticResize() here.
            // SideUiContainer needs to check whether there actually exists side UI to restore,
            // based on its internal states.
            sideUiContainer.onWindowResized(/* canShowSideUi= */ true);
            return;
        }

        // 4. At this point, we don't need to close or re-open side UI, so we'll just resize side
        // UI.
        if (!newSideUiSpecs.equals(currentSideUiSpecs)) {
            commitNewSpecsForStaticResize(newSideUiSpecs, newSideUiSpecs);
        }
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

    /**
     * Update registered {@link SideUiContainer} widths.
     *
     * @param properties {@link SideUiContainer}'s width properties if present.
     * @param suppressAnimations Whether the animation should be suppressed.
     */
    private void updateContainerWidths(
            @Nullable SideUiContainerProperties properties, boolean suppressAnimations) {
        assert properties == null || properties.mWidth >= 0
                : "Requested a negative width for SideUiContainer.";

        // 1. End any existing transitions still in progress. This needs to be done before checking
        // the current specs, since specs aren't fully updated until after all transitions have
        // finished.
        TransitionManager.endTransitions(getRootView());

        // 2. Check if animations should be disabled entirely.
        suppressAnimations =
                suppressAnimations
                        || ChromeFeatureList.sEnableAndroidSidePanelDisableAnimations.getValue();

        // 3. Determine the new SideUiSpecs.
        @Px int windowWidth = getWindowWidth();
        @Px int minWebContentsWidth = ViewUtils.dpToPx(mParentActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        SideUiSpecs currentSideUiSpecs = getCurrentSideUiSpecsInternal();
        SideUiSpecs newSideUiSpecs =
                determineSideUiSpecs(
                        properties, currentSideUiSpecs, windowWidth, minWebContentsWidth);

        // 4. Collect containers whose width needs updating for resize event and transition effect.
        Map<Integer, Integer> updatedSides = new ArrayMap<>(); // side -> width
        for (SideUiContainer container : mSideUiContainers) {
            @AnchorSide int side = container.getAnchorSide();
            int currentWidth = currentSideUiSpecs.getWidth(side);
            int newWidth = newSideUiSpecs.getWidth(side);
            if (currentWidth != newWidth) {
                updatedSides.put(side, newWidth);
            }
        }

        if (!updatedSides.isEmpty()) {
            // If animating, gather all Transitions into a TransitionSet.
            SideUiSpecs deltaSideUiSpecs = new SideUiSpecs(updatedSides);
            @Nullable TransitionSet transitionSet =
                    suppressAnimations
                            ? null
                            : collectTransitions(newSideUiSpecs, deltaSideUiSpecs);
            commitNewSideUiSpecs(newSideUiSpecs, deltaSideUiSpecs, transitionSet);
        }
    }

    private SideUiSpecs getCurrentSideUiSpecsInternal() {
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

    private @Nullable SideUiContainer getSideUiContainerBySide(@AnchorSide int side) {
        for (SideUiContainer container : mSideUiContainers) {
            if (container.getAnchorSide() == side) return container;
        }
        return null;
    }

    /**
     * Determines {@link SideUiSpecs} based on optional request.
     *
     * @param requestedProperties The properties of {@link SideUiContainer} requesting update.
     * @param currentSideUiSpecs Current {@link SideUiSpecs}.
     * @param windowWidth The current window width (in px).
     * @param minWebContentsWidth The minimum width reserved for {@code WebContents} (in px).
     * @return The new {@link SideUiSpecs}.
     */
    private SideUiSpecs determineSideUiSpecs(
            @Nullable SideUiContainerProperties requestedProperties,
            SideUiSpecs currentSideUiSpecs,
            @Px int windowWidth,
            @Px int minWebContentsWidth) {
        int availableWidth = windowWidth - minWebContentsWidth;
        Map<Integer, Integer> sideUiWidths = new ArrayMap<>(); // anchorSide -> width

        // Initialize the widths from the current anchorContainers.
        for (@AnchorSide int side : mAnchorContainers.keySet()) {
            sideUiWidths.put(side, 0);
        }
        for (var container : mSideUiContainers) {
            int width = currentSideUiSpecs.getWidth(container.getAnchorSide());
            int sideUiId = container.getSideUiId();

            // When all the SideUiContainer widths are re-evaluated (i.e. requestedProperties ==
            // null), note that we shouldn't use 0 as the requestWidth when current |width| is 0,
            // since doing so will cause the new SideUi width to be 0 and fail to re-open SideUi
            // even if the window has become wide enough.
            //
            // Therefore, when currentSideUiWidth is 0, we _request_ the SideUi's maximum available
            // width (i.e., windowWidth - minWebContentsWidth) so that determineSideUiSpecs() can
            // return a non-zero width if the window is wide enough.
            int requestWidth = width;
            if (requestedProperties == null) {
                requestWidth = availableWidth;
            } else if (requestedProperties.mSideUiId == sideUiId) {
                requestWidth = requestedProperties.mWidth;
            }
            int newSideUiWidth =
                    container.determineContainerWidth(requestWidth, availableWidth, windowWidth);
            sideUiWidths.put(container.getAnchorSide(), newSideUiWidth);
            availableWidth = Math.max(availableWidth - newSideUiWidth, 0);
        }
        return new SideUiSpecs(sideUiWidths);
    }

    private ViewGroup getRootView() {
        return (ViewGroup) mAnchorContainerParent.getRootView();
    }

    /**
     * Collects Transitions from the SideUiObservers to animate an update to the containers, and
     * returns a TransitionSet that plays all the Transitions together.
     *
     * @param newSideUiSpecs The new complete {@link SideUiSpecs}.
     * @param deltaSideUiSpecs The {@link SideUiSpecs} containing the width of {@link AnchorSide}s
     *     that need updating only.
     */
    // TODO(crbug.com/510059861): Add tests for transition animations.
    private TransitionSet collectTransitions(
            SideUiSpecs newSideUiSpecs, SideUiSpecs deltaSideUiSpecs) {
        // Rather than use a standard Android or Material interpolator, we instead match the desktop
        // impl's curve found at chrome/browser/ui/views/animations/side_panel_animations.cc.
        TimeInterpolator interpolator = PathInterpolatorCompat.create(0.45f, 0f, 0.12f, 1f);
        TransitionSet transitionSet =
                new TransitionSet()
                        .setDuration(TRANSITION_DURATION_MS)
                        .setOrdering(TransitionSet.ORDERING_TOGETHER)
                        .setInterpolator(interpolator);

        for (Map.Entry<Integer, Integer> entry : deltaSideUiSpecs.entrySet()) {
            int side = entry.getKey();
            int width = entry.getValue();
            // Add transitions for the side UI containers.
            ViewGroup anchorContainer = assumeNonNull(mAnchorContainers.get(side));
            transitionSet.addTransition(
                    SideUiContainerTransition.createContainerTransition(
                            anchorContainer, side, width));
        }
        for (SideUiObserver observer : mSideUiObservers) {
            @Nullable Transition observerTransition =
                    observer.onPreSideUiSpecsChange(newSideUiSpecs);
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
     * @param newSideUiSpecs The new, complete {@link SideUiSpecs}.
     * @param deltaSideUiSpecs The {@link SideUiSpecs} containing the width of {@link AnchorSide}s
     *     that need updating only.
     * @param transitionSet The {@link TransitionSet} directing the animation for the update. If
     *     null, then no animation is happening for the update.
     */
    private void commitNewSideUiSpecs(
            SideUiSpecs newSideUiSpecs,
            SideUiSpecs deltaSideUiSpecs,
            @Nullable TransitionSet transitionSet) {
        // Update the side containers, with Transitions if available.
        if (transitionSet != null) {
            commitNewSpecsForAnimatedResize(newSideUiSpecs, deltaSideUiSpecs, transitionSet);
        } else {
            commitNewSpecsForStaticResize(newSideUiSpecs, deltaSideUiSpecs);
        }
    }

    private void commitNewSpecsForAnimatedResize(
            SideUiSpecs newSideUiSpecs, SideUiSpecs deltaSideUiSpecs, TransitionSet transitionSet) {
        for (Map.Entry<Integer, Integer> entry : deltaSideUiSpecs.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            int sideUiWidth = entry.getValue();
            SideUiContainer sideUiContainer = assumeNonNull(getSideUiContainerBySide(anchorSide));
            // Ensure side UI container is attached and, if showing, starts offscreen with the
            // side UI width. If hiding, i.e. side UI width is 0, then setWidth() should be
            // delayed until after the Transition is finished.
            attachSideUiContainerView(sideUiContainer, anchorSide);
            if (sideUiWidth != 0) {
                sideUiContainer.setWidth(sideUiWidth);
            }
            transitionSet.addListener(
                    new TransitionListenerAdapter() {
                        @Override
                        public void onTransitionEnd(Transition transition) {
                            // Detach and close the container after the transition is complete.
                            for (Map.Entry<Integer, Integer> entry : deltaSideUiSpecs.entrySet()) {
                                @AnchorSide int anchorSide = entry.getKey();
                                int sideUiWidth = entry.getValue();
                                SideUiContainer sideUiContainer =
                                        assumeNonNull(getSideUiContainerBySide(anchorSide));
                                if (sideUiWidth == 0) {
                                    detachSideUiContainerView(sideUiContainer);
                                    sideUiContainer.setWidth(0);
                                }
                                sideUiContainer.onContainerResized(sideUiWidth);
                            }
                            for (SideUiObserver observer : mSideUiObservers) {
                                observer.onTransitionEnded(newSideUiSpecs);
                            }
                        }
                    });
        }
        // Trigger a synchronous measure and layout pass on the container to ensure that the
        // starting snapshot for the Transition is updated and accurate. If this is not done,
        // the side panel can have visual bugs where it animates from an incorrect starting
        // point, especially if the window has been resized recently. Updating View attributes,
        // like setting a View's translation, is not enough alone.
        ViewUtils.triggerSynchronousMeasureAndLayout(mAnchorContainerParent);
        TransitionManager.beginDelayedTransition(getRootView(), transitionSet);

        for (Map.Entry<Integer, Integer> entry : deltaSideUiSpecs.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            int sideUiWidth = entry.getValue();
            ViewGroup anchorContainer = assumeNonNull(mAnchorContainers.get(anchorSide));
            SideUiContainerTransition.triggerContainerTransition(
                    anchorContainer, anchorContainer.getWidth(), anchorSide, sideUiWidth);
        }
        for (SideUiObserver observer : mSideUiObservers) {
            observer.onTransitionBegun(newSideUiSpecs);
        }
    }

    private void commitNewSpecsForStaticResize(
            SideUiSpecs newSideUiSpecs, SideUiSpecs deltaSideUiSpecs) {
        // Reset the side UI containers to clear any leftover state from previous Transitions.
        for (var container : mAnchorContainers.values()) {
            SideUiContainerTransition.resetContainer(container);
        }

        for (Map.Entry<Integer, Integer> entry : deltaSideUiSpecs.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            int sideUiWidth = entry.getValue();
            SideUiContainer sideUiContainer = getSideUiContainerBySide(anchorSide);
            if (sideUiContainer == null) continue;

            if (sideUiWidth != 0) {
                attachSideUiContainerView(sideUiContainer, anchorSide);
            } else {
                detachSideUiContainerView(sideUiContainer);
            }
            sideUiContainer.setWidth(sideUiWidth);
            sideUiContainer.onContainerResized(sideUiWidth);
        }

        notifySideUiSpecsChanged(newSideUiSpecs);
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
