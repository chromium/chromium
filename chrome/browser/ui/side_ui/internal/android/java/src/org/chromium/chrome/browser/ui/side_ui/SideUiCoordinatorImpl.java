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
import org.chromium.base.CallbackController;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.util.TokenHolder;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Implementation of {@link SideUiCoordinator}. */
@NullMarked
final class SideUiCoordinatorImpl
        implements SideUiCoordinator,
                ConfigurationChangedObserver,
                BrowserControlsStateProvider.Observer,
                FullscreenManager.Observer {

    private static final long TRANSITION_DURATION_MS = 350L;

    private final Activity mParentActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final TopControlsStacker mTopControlsStacker;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private final FullscreenManager mFullscreenManager;

    private final ViewGroup mAnchorContainerParent;

    private final CallbackController mCallbackController = new CallbackController();
    private @Nullable LayoutStateProvider mLayoutStateProvider;
    private @Nullable LayoutStateObserver mLayoutStateObserver;

    /** Maps {@link AnchorSide} to {@link ViewGroup} where {@link SideUiContainer} is attached. */
    private final Map<@AnchorSide Integer, ViewGroup> mAnchorContainers = new ArrayMap<>();

    /** List of registered {@link SideUiContainer} objects. */
    private final List<SideUiContainer> mSideUiContainers = new ArrayList<>();

    private final SideUiObserverNotifier mSideUiObserverNotifier = new SideUiObserverNotifier();
    private final SideUiTransitionListener mSideUiTransitionListener =
            new SideUiTransitionListener();

    private final SideUiWebContentHairlineManager mWebContentsHairlineManager;
    private final TabModelSelector mTabModelSelector;

    private int mBrowserControlsToken = TokenHolder.INVALID_TOKEN;

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
     * @param layoutStateProviderSupplier Supplier for the {@link LayoutStateProvider}.
     * @param browserControlVisibilityManager The {@link BrowserControlsVisibilityManager} to adjust
     *     for top controls changes.
     * @param fullscreenManager {@link FullscreenManager} to observe fullscreen mode switching.
     * @param topControlsStacker The {@link TopControlsStacker} to calculate heights for top
     *     controls.
     * @param anchorContainerParent The {@link ViewGroup} that is the parent for the side UI
     *     containers.
     * @param leftAnchorContainerStub The {@link ViewStub} for the left-anchored container.
     * @param rightAnchorContainerStub The {@link ViewStub} for the right-anchored container.
     * @param webContentHairlineContainerStub The {@link ViewStub} for the web content hairline
     *     container.
     * @param incognitoStateProvider The {@link IncognitoStateProvider} to observe incognito state.
     * @param tabModelSelector The {@link TabModelSelector} to query tabs.
     */
    /* package */ SideUiCoordinatorImpl(
            Activity parentActivity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            BrowserControlsVisibilityManager browserControlVisibilityManager,
            FullscreenManager fullscreenManager,
            TopControlsStacker topControlsStacker,
            ViewGroup anchorContainerParent,
            ViewStub leftAnchorContainerStub,
            ViewStub rightAnchorContainerStub,
            ViewStub webContentHairlineContainerStub,
            IncognitoStateProvider incognitoStateProvider,
            TabModelSelector tabModelSelector) {
        mParentActivity = parentActivity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mBrowserControlsVisibilityManager = browserControlVisibilityManager;
        mFullscreenManager = fullscreenManager;
        mTopControlsStacker = topControlsStacker;
        mAnchorContainerParent = anchorContainerParent;
        mTabModelSelector = tabModelSelector;

        mBrowserControlsVisibilityDelegate =
                browserControlVisibilityManager.getBrowserVisibilityDelegate();

        ViewGroup leftAnchorContainer = (ViewGroup) leftAnchorContainerStub.inflate();
        ViewGroup rightAnchorContainer = (ViewGroup) rightAnchorContainerStub.inflate();
        assert mAnchorContainerParent == leftAnchorContainer.getParent();
        assert mAnchorContainerParent == rightAnchorContainer.getParent();
        mAnchorContainers.put(AnchorSide.LEFT, leftAnchorContainer);
        mAnchorContainers.put(AnchorSide.RIGHT, rightAnchorContainer);

        webContentHairlineContainerStub.setLayoutResource(
                R.layout.side_ui_web_content_hairline_container);
        SideUiWebContentHairlineContainer webContentHairlineContainer =
                (SideUiWebContentHairlineContainer) webContentHairlineContainerStub.inflate();
        mWebContentsHairlineManager =
                new SideUiWebContentHairlineManager(
                        browserControlVisibilityManager,
                        /* sideUiStateProvider= */ this,
                        webContentHairlineContainer,
                        incognitoStateProvider,
                        topControlsStacker);

        // TODO(crbug.com/540566058): Investigate if we need to recolor the anchor containers when
        //  toggling Incognito state.

        layoutStateProviderSupplier.onAvailable(
                mCallbackController.makeCancelable(this::onLayoutStateProviderAvailable));
        browserControlVisibilityManager.addObserver(this);
        mFullscreenManager.addObserver(this);
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

        // It's possible to request unregistering a SideUiContainer before it's registered.
        // For example, if a SideUiContainer needs to be registered _after_ the async native
        // initialization, but ChromeActivity is destroyed before the async task is completed.
        //
        // Therefore, we shouldn't assert that the given SideUiContainer is already registered.
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
        releasePersistentShowingToken();
        if (mLayoutStateProvider != null && mLayoutStateObserver != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateProvider = null;
            mLayoutStateObserver = null;
        }
        mCallbackController.destroy();
        mSideUiContainers.clear();
        mBrowserControlsVisibilityManager.removeObserver(this);
        mFullscreenManager.removeObserver(this);
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
    public SideUiSpecs getExpectedSideUiSpecsForTab(Tab tab) {
        ThreadUtils.assertOnUiThread();
        @Px int windowWidth = getWindowWidth();
        @Px int minWebContentsWidth = ViewUtils.dpToPx(mParentActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        boolean isFullscreen = mFullscreenManager.getPersistentFullscreenMode();
        return determineSideUiSpecs(windowWidth, minWebContentsWidth, isFullscreen, tab);
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
        boolean isFullscreen = mFullscreenManager.getPersistentFullscreenMode();
        var sideUiShowability =
                determineSideUiShowability(windowWidth, minWebContentsWidth, isFullscreen);

        return sideUiShowability.mShowableSideUiIds.contains(sideUiId);
    }

    private void onLayoutStateProviderAvailable(LayoutStateProvider layoutStateProvider) {
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateObserver =
                new LayoutStateObserver() {
                    @Override
                    public void onFinishedShowing(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.HUB) {
                            setHideSideUiView(true);
                        }
                    }

                    @Override
                    public void onStartedHiding(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.HUB) {
                            setHideSideUiView(false);
                        }
                    }
                };
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
        if (mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)) {
            setHideSideUiView(true);
        }
    }

    private void setHideSideUiView(boolean hide) {
        ThreadUtils.assertOnUiThread();
        int visibility = hide ? View.INVISIBLE : View.VISIBLE;
        for (ViewGroup container : mAnchorContainers.values()) {
            if (container.getVisibility() != View.GONE) {
                container.setVisibility(visibility);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of SideUiStateProvider Implementation                                    //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // BrowserControlsStateProvider.Observer implementation:
    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateUiInternal(new UiUpdateRequest(/* sideUiId= */ null, /* suppressAnimations= */ true));
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            boolean topControlsMinHeightChanged,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean bottomControlsMinHeightChanged,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        updateUiInternal(new UiUpdateRequest(/* sideUiId= */ null, /* suppressAnimations= */ true));
    }

    // ConfigurationChangedObserver Implementation
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        updateUiInternal(new UiUpdateRequest(/* sideUiId= */ null, /* suppressAnimations= */ true));
    }

    // FullscreenManager.Observer implementation:
    @Override
    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
        updateUiInternal(new UiUpdateRequest(/* sideUiId= */ null, /* suppressAnimations= */ true));
    }

    @Override
    public void onExitFullscreen(Tab tab) {
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
            @HeightType int oldHeightType = oldSideUiSpecs.getHeightType(anchorSide);
            @HeightType int newHeightType = newSideUiSpecs.getHeightType(anchorSide);
            if (newWidth != oldWidth || oldHeightType != newHeightType) {
                container.onUiUpdateCompleted(oldWidth, newWidth, oldHeightType, newHeightType);
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

        // 3. Determine the new SideUiShowability, SideUiSpecs, and AnchorContainerTopMargins.
        @Px int windowWidth = getWindowWidth();
        @Px int minWebContentsWidth = ViewUtils.dpToPx(mParentActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        boolean isFullscreen = mFullscreenManager.getPersistentFullscreenMode();
        SideUiShowability newSideUiShowability =
                determineSideUiShowability(windowWidth, minWebContentsWidth, isFullscreen);
        SideUiSpecs newSideUiSpecs =
                determineSideUiSpecs(windowWidth, minWebContentsWidth, isFullscreen);
        AnchorContainerTopMargins newTopMargins =
                determineAnchorContainerTopMargins(newSideUiSpecs);

        // 4. Collect containers whose width needs updating for resize event and transition effect.
        SideUiSpecs currentSideUiSpecs = getCurrentSideUiSpecsInternal();
        SideUiSpecs sideUiSpecsDiff = newSideUiSpecs.diffAgainst(currentSideUiSpecs);
        AnchorContainerTopMargins currentTopMargins = getCurrentAnchorContainerTopMargins();
        AnchorContainerTopMargins topMarginDiff = newTopMargins.diffAgainst(currentTopMargins);

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

        // 7. Update browser controls visibility constraint.
        updateBrowserControlsVisibility(newSideUiSpecs);

        // 8. Commit the new SideUiSpecs.
        if (!sideUiSpecsDiff.isEmpty() || !topMarginDiff.isEmpty()) {
            var uiUpdateSpecs =
                    new SideUiUpdateSpecs(
                            currentSideUiSpecs, newSideUiSpecs, sideUiSpecsDiff, topMarginDiff);
            @Nullable TransitionSet transitionSet =
                    suppressAnimations ? null : collectTransitions(uiUpdateSpecs);
            commitNewSideUiSpecs(uiUpdateSpecs, transitionSet);
            mWebContentsHairlineManager.update();
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
        Map<@AnchorSide Integer, SideUiSize> anchorContainerSpecs = new ArrayMap<>();
        for (Map.Entry<@AnchorSide Integer, ViewGroup> entry : mAnchorContainers.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            ViewGroup anchorContainer = entry.getValue();
            @Px
            int anchorContainerWidth =
                    anchorContainer.getVisibility() == View.GONE ? 0 : anchorContainer.getWidth();
            @HeightType int heightType = getCurrentHeightType(anchorSide);
            anchorContainerSpecs.put(anchorSide, new SideUiSize(anchorContainerWidth, heightType));
        }

        return new SideUiSpecs(anchorContainerSpecs);
    }

    private @Px int getTopMarginForHeightType(@HeightType int heightType) {
        // In persistent fullscreen mode, the top controls are hidden, so the anchor containers'
        // top margin will be 0.
        if (mFullscreenManager.getPersistentFullscreenMode()) return 0;

        // Otherwise, we determine the top controls height, and therefore the anchor containers'
        // top margin, from mTopControlsStacker. Currently, all the supported SideUiContainers lock
        // top controls, meaning we do not need to account for the scroll offset here. If top
        // controls are not locked for a given SideUiContainer, this logic will need to change.
        return switch (heightType) {
            case HeightType.TOOLBAR ->
                    mTopControlsStacker.getHeightFromLayerBottomToTop(TopControlType.TABSTRIP);
            case HeightType.WEB_CONTENTS -> getTopMarginForWebContentsHeightType();
            default ->
                    // includes HeightType.NOT_APPLICABLE
                    throw new IllegalStateException(
                            "Unable to get top margin for HeightType: " + heightType);
        };
    }

    private @Px int getTopMarginForWebContentsHeightType() {
        int totalHeight = mTopControlsStacker.getVisibleTopControlsTotalHeight();
        // When the bookmarks bar is showing, its layer bakes in the hairline height, causing the
        // total height to extend past the top of the hairline. Subtract the hairline height so the
        // container aligns with the top of the hairline stroke. This caused a bug where the rounded
        // corner was not aligned with the top controls hairline. See crbug.com/539662382.
        // TODO(crbug.com/532218047): Once the toolbar refactor is complete, this logic should
        //  be safe to remove.
        if (!ChromeFeatureList.sToolbarProgressBarRefactor.isEnabled()
                && mTopControlsStacker.isLayerAtBottom(TopControlType.BOOKMARK_BAR)) {
            int hairlineHeight = mBrowserControlsVisibilityManager.getTopControlsHairlineHeight();
            totalHeight = Math.max(0, totalHeight - hairlineHeight);
        }
        return totalHeight;
    }

    private @HeightType int getCurrentHeightType(@AnchorSide int anchorSide) {
        var anchorContainerTopMargins = getCurrentAnchorContainerTopMargins();
        Integer topMargin = anchorContainerTopMargins.get(anchorSide);
        if (topMargin == null) return HeightType.NOT_APPLICABLE;

        return topMargin.equals(getTopMarginForHeightType(HeightType.TOOLBAR))
                ? HeightType.TOOLBAR
                : HeightType.WEB_CONTENTS;
    }

    private AnchorContainerTopMargins getCurrentAnchorContainerTopMargins() {
        Map<@AnchorSide Integer, Integer> topMargins = new ArrayMap<>();
        for (Map.Entry<@AnchorSide Integer, ViewGroup> entry : mAnchorContainers.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            ViewGroup anchorContainer = entry.getValue();
            if (anchorContainer.getVisibility() == View.GONE || anchorContainer.getWidth() == 0) {
                continue;
            }
            MarginLayoutParams lp = (MarginLayoutParams) anchorContainer.getLayoutParams();
            topMargins.put(anchorSide, lp.topMargin);
        }
        return new AnchorContainerTopMargins(topMargins);
    }

    /**
     * Determines {@link SideUiShowability}.
     *
     * @param windowWidth The current window width (in px).
     * @param minWebContentsWidth The minimum width reserved for {@code WebContents} (in px).
     * @param isFullscreen Whether the app is in persistent fullscreen mode.
     * @return The new {@link SideUiShowability}.
     */
    private SideUiShowability determineSideUiShowability(
            @Px int windowWidth, @Px int minWebContentsWidth, boolean isFullscreen) {
        int availableWidth = windowWidth - minWebContentsWidth;
        List<@SideUiId Integer> showableSideUiIds = new ArrayList<>();
        List<@SideUiId Integer> unShowableSideUiIds = new ArrayList<>();

        @Nullable Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) {
            for (var container : mSideUiContainers) {
                unShowableSideUiIds.add(container.getSideUiId());
            }
            return new SideUiShowability(showableSideUiIds, unShowableSideUiIds);
        }

        for (var container : mSideUiContainers) {
            int showableWidth =
                    container.determineShowableSize(availableWidth, windowWidth, isFullscreen)
                            .mWidth;
            if (showableWidth > 0) {
                showableSideUiIds.add(container.getSideUiId());
            } else {
                unShowableSideUiIds.add(container.getSideUiId());
            }

            // If a SideUiContainer is showable and has content to show, it will be shown.
            // Therefore, we should subtract the showable width from the available width.
            if (showableWidth > 0 && container.hasContentToShow(currentTab)) {
                availableWidth = Math.max(availableWidth - showableWidth, 0);
            }
        }

        return new SideUiShowability(showableSideUiIds, unShowableSideUiIds);
    }

    /**
     * Determines {@link SideUiSpecs} for the current active UI.
     *
     * @param windowWidth The current window width (in px).
     * @param minWebContentsWidth The minimum width reserved for {@code WebContents} (in px).
     * @param isFullscreen Whether the app is in persistent fullscreen mode.
     * @return The new {@link SideUiSpecs}.
     */
    private SideUiSpecs determineSideUiSpecs(
            @Px int windowWidth, @Px int minWebContentsWidth, boolean isFullscreen) {
        return determineSideUiSpecs(
                windowWidth, minWebContentsWidth, isFullscreen, mTabModelSelector.getCurrentTab());
    }

    /**
     * Determines {@link SideUiSpecs} for a given {@link Tab}.
     *
     * @param windowWidth The current window width (in px).
     * @param minWebContentsWidth The minimum width reserved for {@code WebContents} (in px).
     * @param isFullscreen Whether the app is in persistent fullscreen mode.
     * @param tab The target {@link Tab} to compute specs for, or {@code null} for the current tab.
     * @return The new {@link SideUiSpecs}.
     */
    private SideUiSpecs determineSideUiSpecs(
            @Px int windowWidth,
            @Px int minWebContentsWidth,
            boolean isFullscreen,
            @Nullable Tab tab) {
        int availableWidth = windowWidth - minWebContentsWidth;
        Map<@AnchorSide Integer, SideUiSize> sideUiSpecs = new ArrayMap<>(); // anchorSide -> spec

        // Initialize the specs from the current anchorContainers.
        for (@AnchorSide int side : mAnchorContainers.keySet()) {
            sideUiSpecs.put(side, new SideUiSize(0, HeightType.NOT_APPLICABLE));
        }

        if (tab == null) {
            return new SideUiSpecs(sideUiSpecs);
        }

        for (var container : mSideUiContainers) {
            SideUiSize newSideUiSize =
                    container.hasContentToShow(tab)
                            ? container.determineShowableSize(
                                    availableWidth, windowWidth, isFullscreen)
                            : new SideUiSize(0, HeightType.NOT_APPLICABLE);
            sideUiSpecs.put(container.getAnchorSide(), newSideUiSize);
            availableWidth = Math.max(availableWidth - newSideUiSize.mWidth, 0);
        }
        return new SideUiSpecs(sideUiSpecs);
    }

    /**
     * Determines the {@link AnchorContainerTopMargin} for {@link AnchorSide}s.
     *
     * <p>This derives the pixel value for each anchor container's top margin from the {@link
     * HeightType} enum in {@link SideUiSpecs}.
     */
    private AnchorContainerTopMargins determineAnchorContainerTopMargins(SideUiSpecs sideUiSpecs) {
        Map<@AnchorSide Integer, Integer> topMargins = new ArrayMap<>();

        for (Map.Entry<@AnchorSide Integer, SideUiSize> entry : sideUiSpecs.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            @HeightType int heightType = entry.getValue().mHeightType;
            if (heightType == HeightType.NOT_APPLICABLE) continue;

            topMargins.put(anchorSide, getTopMarginForHeightType(heightType));
        }

        return new AnchorContainerTopMargins(topMargins);
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

        for (Map.Entry<@AnchorSide Integer, SideUiSize> entry :
                uiUpdateSpecs.mSpecsDiff.entrySet()) {
            int side = entry.getKey();
            int newWidth = entry.getValue().mWidth;
            int oldWidth = uiUpdateSpecs.mCurrentSpecs.getWidth(side);
            // Add transitions for the side UI containers.
            ViewGroup anchorContainer = assumeNonNull(mAnchorContainers.get(side));
            transitionSet.addTransition(
                    SideUiContainerTransition.createContainerTransition(
                            anchorContainer, side, oldWidth, newWidth));
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
        // Whether both the width and height gets updated. The animation will be suppressed if true.
        boolean willUpdateBothWidthHeight = false;
        for (var marginDiff : uiUpdateSpecs.mTopMarginDiff.entrySet()) {
            @AnchorSide int side = marginDiff.getKey();
            @Px int topMargin = marginDiff.getValue();

            // The following check is for catching crbug.com/544876870.
            // Side panel's anchor container should always have a positive top margin, if the window
            // isn't in fullscreen or on Android Automotive.
            // The symptom of crbug.com/544876870 is that side panel's anchor container has 0 top
            // margin. The bug was hard to reproduce so we only landed a speculative fix. The
            // check here will catch the bug if it happens again.
            var sideUiContainer = assumeNonNull(getSideUiContainerBySide(side));
            if (sideUiContainer.getSideUiId() == SideUiId.SIDE_PANEL
                    && !mFullscreenManager.getPersistentFullscreenMode()
                    && !DeviceInfo.isAutomotive()
                    && topMargin <= 0) {
                throw new IllegalStateException(
                        "Side panel's anchor container should have a positive top margin. See"
                                + " crbug.com/544876870");
            }

            boolean willUpdateWidth =
                    (uiUpdateSpecs.mCurrentSpecs.getWidth(side)
                            != uiUpdateSpecs.mNewSpecs.getWidth(side));

            // HeightType update from/to NOT_APPLICABLE means the SideUiContainer is
            // opened/closed, which has animation support. Only a change between
            // TOOLBAR and WEB_CONTENTS or one in pixel values (due to top controls
            // showing/hiding) doesn't have animation support. So we only care about the
            // latter here.
            boolean willUpdateHeight =
                    (uiUpdateSpecs.mCurrentSpecs.getHeightType(side) != HeightType.NOT_APPLICABLE
                            && uiUpdateSpecs.mNewSpecs.getHeightType(side)
                                    != HeightType.NOT_APPLICABLE);
            willUpdateBothWidthHeight |= (willUpdateWidth && willUpdateHeight);
            ViewGroup anchorContainer = assumeNonNull(mAnchorContainers.get(side));
            var layoutParams = (MarginLayoutParams) anchorContainer.getLayoutParams();
            layoutParams.topMargin = topMargin;
            anchorContainer.setLayoutParams(layoutParams);
        }

        if (transitionSet != null && !willUpdateBothWidthHeight) {
            commitNewSpecsForAnimatedResize(uiUpdateSpecs, transitionSet);
        } else {
            commitNewSpecsForStaticResize(uiUpdateSpecs);
        }
    }

    private void commitNewSpecsForAnimatedResize(
            SideUiUpdateSpecs uiUpdateSpecs, TransitionSet transitionSet) {
        SideUiSpecs newSideUiSpecs = uiUpdateSpecs.mNewSpecs;
        SideUiSpecs sideUiSpecsDiff = uiUpdateSpecs.mSpecsDiff;
        SideUiSpecs currentSideUiSpecs = uiUpdateSpecs.mCurrentSpecs;

        for (Map.Entry<@AnchorSide Integer, SideUiSize> entry : sideUiSpecsDiff.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            int newWidth = entry.getValue().mWidth;
            int oldWidth = currentSideUiSpecs.getWidth(anchorSide);
            SideUiContainer sideUiContainer = assumeNonNull(getSideUiContainerBySide(anchorSide));
            // Ensure side UI container is attached.
            attachSideUiContainerView(sideUiContainer, anchorSide);

            // Only set the width immediately if it's a show event (0 -> non-zero).
            // For resize, we set it after beginDelayedTransition.
            // For hide, it's set after the transition ends.
            if (newWidth != 0 && oldWidth == 0) {
                sideUiContainer.setWidth(newWidth);
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
                        for (Map.Entry<@AnchorSide Integer, SideUiSize> entry :
                                uiUpdateSpecs.mSpecsDiff.entrySet()) {
                            @AnchorSide int anchorSide = entry.getKey();
                            @Px int newSideUiWidth = entry.getValue().mWidth;
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

        // Apply target layout changes (setting width for resize, translation for slide) after
        // capturing the starting state with beginDelayedTransition.
        for (Map.Entry<@AnchorSide Integer, SideUiSize> entry : sideUiSpecsDiff.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            int newWidth = entry.getValue().mWidth;
            int oldWidth = currentSideUiSpecs.getWidth(anchorSide);
            ViewGroup anchorContainer = assumeNonNull(mAnchorContainers.get(anchorSide));
            SideUiContainer sideUiContainer = assumeNonNull(getSideUiContainerBySide(anchorSide));
            SideUiContainerTransition.triggerContainerTransition(
                    anchorContainer, sideUiContainer, anchorSide, oldWidth, newWidth);
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

        for (Map.Entry<@AnchorSide Integer, SideUiSize> entry : sideUiSpecsDiff.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            int newSideUiWidth = entry.getValue().mWidth;
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
        targetParent.setVisibility(
                mLayoutStateProvider != null && mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)
                        ? View.INVISIBLE
                        : View.VISIBLE);
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

    private void updateBrowserControlsVisibility(SideUiSpecs newSideUiSpecs) {
        boolean shouldLockTopControls = shouldLockTopControls(newSideUiSpecs);
        if (!shouldLockTopControls) {
            releasePersistentShowingToken();
            return;
        }

        if (mBrowserControlsToken == TokenHolder.INVALID_TOKEN) {
            mBrowserControlsToken = mBrowserControlsVisibilityDelegate.showControlsPersistent();
        }
    }

    private boolean shouldLockTopControls(SideUiSpecs sideUiSpecs) {
        for (var container : mSideUiContainers) {
            if (container.shouldLockTopControls()
                    && sideUiSpecs.getWidth(container.getAnchorSide()) > 0) {
                return true;
            }
        }
        return false;
    }

    private void releasePersistentShowingToken() {
        if (mBrowserControlsToken != TokenHolder.INVALID_TOKEN) {
            mBrowserControlsVisibilityDelegate.releasePersistentShowingToken(mBrowserControlsToken);
            mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private @Px int getWindowWidth() {
        return WindowMetricsCalculator.getOrCreate()
                .computeCurrentWindowMetrics(mParentActivity)
                .getBounds()
                .width();
    }
}
