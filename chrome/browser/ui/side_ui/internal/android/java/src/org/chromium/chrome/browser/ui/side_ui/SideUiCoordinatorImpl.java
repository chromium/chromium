// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.TimeInterpolator;
import android.app.Activity;
import android.content.res.Configuration;
import android.transition.Transition;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.util.ArrayMap;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewParent;
import android.view.ViewStub;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.animation.PathInterpolatorCompat;
import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
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

    private final NonNullObservableSupplier<Integer> mTopMarginSupplier;
    private final Callback<Integer> mTopMarginObserver;

    /** Maps {@link AnchorSide} to {@link ViewGroup} where {@link SideUiContainer} is attached. */
    private final Map<@AnchorSide Integer, ViewGroup> mAnchorContainers = new ArrayMap<>();

    /** List of registered {@link SideUiContainer} objects. */
    private final List<SideUiContainer> mSideUiContainers = new ArrayList<>();

    private final SideUiObserverNotifier mSideUiObserverNotifier = new SideUiObserverNotifier();
    private final SideUiTransitionListener mSideUiTransitionListener =
            new SideUiTransitionListener();

    private final SideUiWebContentHairlineManager mWebContentsHairlineManager;

    /**
     * Whether {@link #updateUiInternal} is in progress.
     *
     * <p>This is used to prevent re-entrancy into {@link #updateUiInternal}.
     */
    private boolean mIsUpdatingUi;

    /**
     * Constructor for a {@link SideUiCoordinatorImpl}.
     *
     * @param parentActivity The {@link Activity} containing all Side UIs.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for {@code
     *     parentActivity}.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} to adjust for
     *     top controls changes.
     * @param anchorContainerParent The {@link ViewGroup} that is the parent for the side UI
     *     containers.
     * @param leftAnchorContainerStub The {@link ViewStub} for the left-anchored container.
     * @param rightAnchorContainerStub The {@link ViewStub} for the right-anchored container.
     * @param webContentHairlineContainerStub The {@link ViewStub} for the web content hairline
     *     container.
     * @param topMarginSupplier The supplier for the Side UI's top margin.
     */
    /* package */ SideUiCoordinatorImpl(
            Activity parentActivity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            BrowserControlsStateProvider browserControlsStateProvider,
            ViewGroup anchorContainerParent,
            ViewStub leftAnchorContainerStub,
            ViewStub rightAnchorContainerStub,
            ViewStub webContentHairlineContainerStub,
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

        webContentHairlineContainerStub.setLayoutResource(
                R.layout.side_ui_web_content_hairline_container);
        SideUiWebContentHairlineContainer webContentHairlineContainer =
                (SideUiWebContentHairlineContainer) webContentHairlineContainerStub.inflate();
        mWebContentsHairlineManager =
                new SideUiWebContentHairlineManager(
                        /* sideUiStateProvider= */ this, webContentHairlineContainer);

        mActivityLifecycleDispatcher.register(this);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start of SideUiCoordinator Implementation                                    //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    @Override
    public void registerSideUiContainer(SideUiContainer sideUiContainer) {
        ThreadUtils.assertOnUiThread();
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
        ThreadUtils.assertOnUiThread();
        assert mSideUiContainers.contains(sideUiContainer)
                : "Unregistering unknown SideUiContainer.";
        mSideUiContainers.remove(sideUiContainer);
    }

    @Override
    public void updateUi(UiUpdateRequest request) {
        ThreadUtils.assertOnUiThread();
        updateUiInternal(request);
    }

    @Override
    public void endAnimations() {
        ThreadUtils.assertOnUiThread();
        mSideUiTransitionListener.endTransitions();
    }

    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        mSideUiContainers.clear();
        mTopMarginSupplier.removeObserver(mTopMarginObserver);
        mWebContentsHairlineManager.destroy();
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
        ThreadUtils.assertOnUiThread();
        mSideUiObserverNotifier.addObserver(observer);
    }

    @Override
    public void removeObserver(SideUiObserver observer) {
        ThreadUtils.assertOnUiThread();
        mSideUiObserverNotifier.removeObserver(observer);
    }

    @Override
    public SideUiSpecs getCurrentSideUiSpecs() {
        ThreadUtils.assertOnUiThread();
        return getCurrentSideUiSpecsInternal();
    }

    @Override
    public boolean isSideUiShowing(@SideUiId int sideUiId) {
        ThreadUtils.assertOnUiThread();
        var sideUiContainer = getSideUiContainerById(sideUiId);
        if (sideUiContainer == null) {
            return false;
        }

        var currentSideUiSpecs = getCurrentSideUiSpecsInternal();
        return currentSideUiSpecs.getWidth(sideUiContainer.getAnchorSide()) > 0;
    }

    @Override
    public boolean canShowSideUi(@SideUiId int sideUiId) {
        ThreadUtils.assertOnUiThread();
        @Px int windowWidth = getWindowWidth();
        @Px int minWebContentsWidth = ViewUtils.dpToPx(mParentActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        var sideUiShowability = determineSideUiShowability(windowWidth, minWebContentsWidth);

        return sideUiShowability.mShowableSideUiIds.contains(sideUiId);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of SideUiStateProvider Implementation                                    //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // ConfigurationChangedObserver Implementation
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        updateUiInternal(new UiUpdateRequest(/* sideUiId= */ null, /* suppressAnimations= */ true));
    }

    @VisibleForTesting
    @Nullable SideUiContainer getSideUiContainerById(@SideUiId int id) {
        for (SideUiContainer container : mSideUiContainers) {
            if (container.getSideUiId() == id) return container;
        }
        return null;
    }

    private @Nullable SideUiContainer getSideUiContainerBySide(@AnchorSide int side) {
        for (SideUiContainer container : mSideUiContainers) {
            if (container.getAnchorSide() == side) return container;
        }
        return null;
    }

    private void notifyContainersOnUiUpdateCompleted(
            SideUiSpecs oldSideUiSpecs, SideUiSpecs newSideUiSpecs) {
        for (var container : mSideUiContainers) {
            @AnchorSide int anchorSide = container.getAnchorSide();
            @Px int oldWidth = oldSideUiSpecs.getWidth(anchorSide);
            @Px int newWidth = newSideUiSpecs.getWidth(anchorSide);

            if (newWidth != oldWidth) {
                container.onUiUpdateCompleted(oldWidth, newWidth);
            }
        }
    }

    private boolean hasConflictingAnchorSides(SideUiContainer sideUiContainer) {
        List<@AnchorSide Integer> allocatedAnchorSide = new ArrayList<>();
        @SideUiId int id = sideUiContainer.getSideUiId();

        for (SideUiContainer container : mSideUiContainers) {
            // Existing containers should have no conflict between each other.
            assert !allocatedAnchorSide.contains(container.getAnchorSide());
            allocatedAnchorSide.add(container.getAnchorSide());
            if (id == container.getSideUiId()) return true;
        }
        return allocatedAnchorSide.contains(sideUiContainer.getAnchorSide());
    }

    private void updateUiInternal(UiUpdateRequest request) {
        assert !mIsUpdatingUi : "another UI update is still in progress";
        mIsUpdatingUi = true;

        // 1. End any existing transitions still in progress. This needs to be done before checking
        // the current specs, since specs aren't fully updated until after all transitions have
        // finished.
        mSideUiTransitionListener.endTransitions();

        // 2. Check if animations should be disabled entirely.
        boolean suppressAnimations =
                request.mSuppressAnimations
                        || ChromeFeatureList.sEnableAndroidSidePanelDisableAnimations.getValue();

        // 3. Determine the new SideUiShowability and the new SideUiSpecs.
        @Px int windowWidth = getWindowWidth();
        @Px int minWebContentsWidth = ViewUtils.dpToPx(mParentActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        SideUiShowability newSideUiShowability =
                determineSideUiShowability(windowWidth, minWebContentsWidth);
        SideUiSpecs newSideUiSpecs = determineSideUiSpecs(windowWidth, minWebContentsWidth);

        // 4. Collect containers whose width needs updating for resize event and transition effect.
        SideUiSpecs currentSideUiSpecs = getCurrentSideUiSpecsInternal();
        SideUiSpecs sideUiSpecsDiff = newSideUiSpecs.diffAgainst(currentSideUiSpecs);

        // 5. Handle auto-close/auto-restore.
        for (var container : mSideUiContainers) {
            if (request.mSideUiId != null && container.getSideUiId() == request.mSideUiId) {
                // No need to auto-close/auto-restore the requesting SideUi.
                continue;
            }

            @AnchorSide int anchorSide = container.getAnchorSide();
            @Px int currentWidth = currentSideUiSpecs.getWidth(anchorSide);
            @Px int newWidth = newSideUiSpecs.getWidth(anchorSide);
            if (currentWidth != 0 && newWidth == 0) {
                container.onWillAutoClose();
            } else if (currentWidth == 0 && newWidth != 0) {
                container.onWillAutoRestore();
            }
        }

        // 6. Notify SideUiObservers of the new SideUiShowability.
        mSideUiObserverNotifier.notifySideUiShowability(newSideUiShowability);

        // 7. Commit the new SideUiSpecs.
        if (!sideUiSpecsDiff.isEmpty()) {
            var uiUpdateSpecs =
                    new SideUiUpdateSpecs(currentSideUiSpecs, newSideUiSpecs, sideUiSpecsDiff);
            @Nullable TransitionSet transitionSet =
                    suppressAnimations ? null : collectTransitions(uiUpdateSpecs);
            commitNewSideUiSpecs(uiUpdateSpecs, transitionSet);
        }

        mIsUpdatingUi = false;
    }

    private SideUiSpecs getCurrentSideUiSpecsInternal() {
        // Note: When a View's visibility is changed to View.GONE, it won't be laid out so Android
        // won't update the View's internal states tracking its size. This means View.getWidth() can
        // return a stale value when the visibility is View.GONE.
        //
        // Therefore, we need to explicitly check if the visibility is View.GONE, and if so, return
        // 0.
        Map<@AnchorSide Integer, Integer> anchorContainerWidths = new ArrayMap<>();
        for (Map.Entry<@AnchorSide Integer, ViewGroup> entry : mAnchorContainers.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            ViewGroup anchorContainer = entry.getValue();
            @Px
            int anchorContainerWidth =
                    anchorContainer.getVisibility() == View.GONE ? 0 : anchorContainer.getWidth();
            anchorContainerWidths.put(anchorSide, anchorContainerWidth);
        }

        return new SideUiSpecs(anchorContainerWidths);
    }

    /**
     * Determines {@link SideUiShowability}.
     *
     * @param windowWidth The current window width (in px).
     * @param minWebContentsWidth The minimum width reserved for {@code WebContents} (in px).
     * @return The new {@link SideUiShowability}.
     */
    private SideUiShowability determineSideUiShowability(
            @Px int windowWidth, @Px int minWebContentsWidth) {
        int availableWidth = windowWidth - minWebContentsWidth;
        List<@SideUiId Integer> showableSideUiIds = new ArrayList<>();
        List<@SideUiId Integer> unShowableSideUiIds = new ArrayList<>();

        for (var container : mSideUiContainers) {
            int showableWidth = container.determineShowableWidth(availableWidth, windowWidth);
            if (showableWidth > 0) {
                showableSideUiIds.add(container.getSideUiId());
            } else {
                unShowableSideUiIds.add(container.getSideUiId());
            }

            // If a SideUiContainer is showable and has content to show, it will be shown.
            // Therefore, we should subtract the showable width from the available width.
            if (showableWidth > 0 && container.hasContentToShow()) {
                availableWidth = Math.max(availableWidth - showableWidth, 0);
            }
        }

        return new SideUiShowability(showableSideUiIds, unShowableSideUiIds);
    }

    /**
     * Determines {@link SideUiSpecs}.
     *
     * @param windowWidth The current window width (in px).
     * @param minWebContentsWidth The minimum width reserved for {@code WebContents} (in px).
     * @return The new {@link SideUiSpecs}.
     */
    private SideUiSpecs determineSideUiSpecs(@Px int windowWidth, @Px int minWebContentsWidth) {
        int availableWidth = windowWidth - minWebContentsWidth;
        Map<@AnchorSide Integer, Integer> sideUiWidths = new ArrayMap<>(); // anchorSide -> width

        // Initialize the widths from the current anchorContainers.
        for (@AnchorSide int side : mAnchorContainers.keySet()) {
            sideUiWidths.put(side, 0);
        }
        for (var container : mSideUiContainers) {
            int newSideUiWidth =
                    container.hasContentToShow()
                            ? container.determineShowableWidth(availableWidth, windowWidth)
                            : 0;
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
     * @param uiUpdateSpecs See {@link SideUiUpdateSpecs}.
     */
    private TransitionSet collectTransitions(SideUiUpdateSpecs uiUpdateSpecs) {
        // Rather than use a standard Android or Material interpolator, we instead match the desktop
        // impl's curve found at chrome/browser/ui/views/animations/side_panel_animations.cc.
        TimeInterpolator interpolator = PathInterpolatorCompat.create(0.45f, 0f, 0.12f, 1f);
        TransitionSet transitionSet =
                new TransitionSet()
                        .setDuration(TRANSITION_DURATION_MS)
                        .setOrdering(TransitionSet.ORDERING_TOGETHER)
                        .setInterpolator(interpolator);

        for (Map.Entry<@AnchorSide Integer, Integer> entry : uiUpdateSpecs.mSpecsDiff.entrySet()) {
            int side = entry.getKey();
            int width = entry.getValue();
            // Add transitions for the side UI containers.
            ViewGroup anchorContainer = assumeNonNull(mAnchorContainers.get(side));
            transitionSet.addTransition(
                    SideUiContainerTransition.createContainerTransition(
                            anchorContainer, side, width));
        }

        List<Transition> transitions =
                mSideUiObserverNotifier.notifyPreSideUiSpecsChange(uiUpdateSpecs.mNewSpecs);
        for (var transition : transitions) {
            transitionSet.addTransition(transition);
        }

        return transitionSet;
    }

    /**
     * Commits the newly calculated {@link SideUiSpecs} for {@link SideUiContainer}s.
     *
     * <p>This method will perform static resizing or animated resizing, depending on the presence
     * of the given {@code transitionSet}.
     *
     * @param uiUpdateSpecs See {@link SideUiUpdateSpecs}.
     * @param transitionSet The {@link TransitionSet} directing the animation for the update. If
     *     null, then no animation is happening for the update.
     */
    private void commitNewSideUiSpecs(
            SideUiUpdateSpecs uiUpdateSpecs, @Nullable TransitionSet transitionSet) {
        if (transitionSet != null) {
            commitNewSpecsForAnimatedResize(uiUpdateSpecs, transitionSet);
        } else {
            commitNewSpecsForStaticResize(uiUpdateSpecs);
        }
    }

    private void commitNewSpecsForAnimatedResize(
            SideUiUpdateSpecs uiUpdateSpecs, TransitionSet transitionSet) {
        SideUiSpecs newSideUiSpecs = uiUpdateSpecs.mNewSpecs;
        SideUiSpecs sideUiSpecsDiff = uiUpdateSpecs.mSpecsDiff;

        for (Map.Entry<@AnchorSide Integer, Integer> entry : sideUiSpecsDiff.entrySet()) {
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
        }

        ViewGroup sceneRoot = getRootView();
        mSideUiTransitionListener.startListening(
                sceneRoot,
                uiUpdateSpecs,
                /* onTransitionEndCallback= */ new Callback<SideUiUpdateSpecs>() {
                    @Override
                    public void onResult(SideUiUpdateSpecs uiUpdateSpecs) {
                        // Detach and close the container after the transition is complete.
                        for (Map.Entry<@AnchorSide Integer, Integer> entry :
                                uiUpdateSpecs.mSpecsDiff.entrySet()) {
                            @AnchorSide int anchorSide = entry.getKey();
                            @Px int newSideUiWidth = entry.getValue();
                            SideUiContainer sideUiContainer =
                                    assumeNonNull(getSideUiContainerBySide(anchorSide));
                            if (newSideUiWidth == 0) {
                                detachSideUiContainerView(sideUiContainer);
                                sideUiContainer.setWidth(0);
                            }
                        }

                        notifyContainersOnUiUpdateCompleted(
                                uiUpdateSpecs.mCurrentSpecs, uiUpdateSpecs.mNewSpecs);
                        mSideUiObserverNotifier.notifyTransitionEnded(uiUpdateSpecs.mNewSpecs);
                    }
                });
        transitionSet.addListener(mSideUiTransitionListener);

        // Trigger a synchronous measure and layout pass on the container to ensure that the
        // starting snapshot for the Transition is updated and accurate. If this is not done,
        // the side panel can have visual bugs where it animates from an incorrect starting
        // point, especially if the window has been resized recently. Updating View attributes,
        // like setting a View's translation, is not enough alone.
        ViewUtils.triggerSynchronousMeasureAndLayout(mAnchorContainerParent);
        TransitionManager.beginDelayedTransition(getRootView(), transitionSet);

        for (Map.Entry<@AnchorSide Integer, Integer> entry : sideUiSpecsDiff.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            int sideUiWidth = entry.getValue();
            ViewGroup anchorContainer = assumeNonNull(mAnchorContainers.get(anchorSide));
            SideUiContainerTransition.triggerContainerTransition(
                    anchorContainer, anchorContainer.getWidth(), anchorSide, sideUiWidth);
        }

        mSideUiObserverNotifier.notifyTransitionBegun(newSideUiSpecs);
    }

    private void commitNewSpecsForStaticResize(SideUiUpdateSpecs uiUpdateSpecs) {
        SideUiSpecs currentSideUiSpecs = uiUpdateSpecs.mCurrentSpecs;
        SideUiSpecs newSideUiSpecs = uiUpdateSpecs.mNewSpecs;
        SideUiSpecs sideUiSpecsDiff = uiUpdateSpecs.mSpecsDiff;

        // Reset the side UI containers to clear any leftover state from previous Transitions.
        for (var container : mAnchorContainers.values()) {
            SideUiContainerTransition.resetContainer(container);
        }

        for (Map.Entry<@AnchorSide Integer, Integer> entry : sideUiSpecsDiff.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            int newSideUiWidth = entry.getValue();
            SideUiContainer sideUiContainer = getSideUiContainerBySide(anchorSide);
            if (sideUiContainer == null) continue;

            if (newSideUiWidth != 0) {
                attachSideUiContainerView(sideUiContainer, anchorSide);
            } else {
                detachSideUiContainerView(sideUiContainer);
            }
            sideUiContainer.setWidth(newSideUiWidth);
        }

        // Trigger a synchronous measure and layout pass to apply the new SideUiSpecs to the layout
        // before we notify SideUiContainers and SideUiObservers.
        //
        // This guarantees the actual layout properties are in sync with the new SideUiSpecs
        // received by SideUiContainers and SideUiObservers.
        //
        // Layout synchronization is also important to support multiple UI update requests in
        // quick succession. For example, GLiC browser tests (in C++) can:
        // (1) disable animations,
        // (2) open the side panel, then
        // (3) immediately close the side panel.
        //
        // Without the synchronous measure and layout pass, (3) can happen before Android's
        // asynchronous layout pass actually opens the side panel for (2) (i.e., applying the
        // SideUiSpecs for (2)). Then during the UI update flow for (3), updateUiInternal()
        // will see the side panel already has 0 width, skip the update, and fail to notify GLiC of
        // the "closed" event.
        //
        // Running a synchronous pass here has little to no overall performance impact since the
        // mAnchorContainerParent subtree will definitely be changed at this point. The synchronous
        // pass just does the work sooner, and the subsequent asynchronous pass scheduled by the
        // Android framework will skip this subtree.
        ViewUtils.triggerSynchronousMeasureAndLayout(mAnchorContainerParent);

        notifyContainersOnUiUpdateCompleted(currentSideUiSpecs, newSideUiSpecs);
        mSideUiObserverNotifier.notifySideUiSpecsChanged(newSideUiSpecs);
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
}
