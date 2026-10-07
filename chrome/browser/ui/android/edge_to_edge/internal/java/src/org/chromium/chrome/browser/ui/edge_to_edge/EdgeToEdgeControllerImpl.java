// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static androidx.core.view.WindowInsetsCompat.Type.systemBars;
import static androidx.core.view.WindowInsetsCompat.Type.tappableElement;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.ColorInt;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager.HomepageStateListener;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager.BackupNavbarInsetsCallSite;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;
import org.chromium.ui.util.TokenHolder;

/**
 * Controls use of the Android Edge To Edge feature that allows an App to draw beneath the Status
 * and Navigation Bars. For Chrome, we intend to sometimes draw under the Status Bar (e.g. for
 * customized NTPs) and/or the Navigation Bar.
 */
@NullMarked
@RequiresApi(VERSION_CODES.R)
public class EdgeToEdgeControllerImpl
        implements EdgeToEdgeController,
                TopInsetProvider,
                BrowserControlsStateProvider.Observer,
                LayoutStateProvider.LayoutStateObserver,
                FullscreenManager.Observer {
    private static final String TAG = "E2E_ControllerImpl";

    /** The outermost view in our view hierarchy that is identified with a resource ID. */
    private static final int ROOT_UI_VIEW_ID = android.R.id.content;

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final TabSupplierObserver mTabSupplierObserver;
    private final ObserverList<EdgeToEdgePadAdjuster> mPadAdjusters = new ObserverList<>();
    private final ObserverList<ChangeObserver> mBottomEdgeChangeObservers = new ObserverList<>();
    private final TabObserver mTabObserver;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final MonotonicObservableSupplier<LayoutManager> mLayoutManagerSupplier;
    private final Callback<LayoutManager> mOnLayoutManagerCallback =
            new ValueChangedCallback<>(this::updateLayoutStateProvider);
    private final FullscreenManager mFullscreenManager;
    private final EdgeToEdgeManager mEdgeToEdgeManager;
    private final EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;
    private final int mEdgeToEdgeToken;

    // Cached rects used for adding under fullscreen.
    private final Rect mCachedWindowVisibleRect = new Rect();
    private final Rect mCachedContentVisibleRect = new Rect();

    /** Multiplier to convert from pixels to DPs. */
    private final float mPxToDp;

    private final boolean mDisablePaddingRootView;

    private final @Nullable EdgeToEdgeOSWrapper mEdgeToEdgeOsWrapper;
    private @Nullable LayoutManager mLayoutManager;

    private @Nullable Tab mCurrentTab;
    private @Nullable WebContentsObserver mWebContentsObserver;

    private boolean mIsBottomChinEnabled;

    /**
     * Whether the system is drawing "toEdge" (i.e. the edge-to-edge wrapper has no bottom padding).
     * This could be due to the current page being opted into bottom edge-to-edge, or a partial
     * edge-to-edge with the bottom chin present.
     */
    private boolean mIsDrawingToBottomEdge;

    /**
     * Whether the page is opted into bottom edge-to-edge. This could be from the web content being
     * opted in, or from the tab showing a native page that supports bottom edge-to-edge.
     */
    private boolean mIsPageOptedIntoBottomEdgeToEdge;

    /**
     * Whether the page should constrain the safe area, which requires the page to be retained
     * within the safe area region. This essentially opts the page out of edge-to-edge, regardless
     * of other flags and values (e.g. |mIsPageOptedIntoBottomEdgeToEdge|)
     */
    private boolean mHasSafeAreaConstraint;

    private InsetObserver mInsetObserver;
    private Insets mSystemInsets = Insets.NONE;
    private Insets mAppliedContentViewPadding = Insets.NONE;
    private @Nullable Insets mKeyboardInsets;
    private final @Nullable WindowInsetsConsumer mWindowInsetsConsumer;
    private boolean mBottomControlsAreVisible;
    private boolean mContentViewScrolling;
    private boolean mPadAdjustersUpdatePending;
    private int mBottomControlsHeight;

    // TODO(crbug.com/498302496): Consolidate TopInsetProvider.Observer with
    // EdgeToEdgeSupplier.ChangeObserver once TopInsetCoordinator is fully deprecated.
    private final ObserverList<TopInsetProvider.Observer> mTopInsetObservers = new ObserverList<>();
    private final boolean mIsEdgeToEdgeRefactorEnabled;
    private boolean mConsumeTopInset;
    private boolean mIsTabSwitcherShowing;
    private boolean mStatusIndicatorVisible;
    private @Nullable HomepageStateListener mHomepageStateListener;

    /**
     * Creates an implementation of the EdgeToEdgeController that will use the Android APIs to allow
     * drawing under the System Gesture Navigation Bar.
     *
     * @param activity The activity to update to allow drawing under System Bars.
     * @param windowAndroid The current {@link WindowAndroid} to allow drawing under System Bars.
     * @param tabObservableSupplier A supplier for Tab changes so this implementation can adjust
     *     whether to draw under or not for each page.
     * @param edgeToEdgeOsWrapper An optional wrapper for OS calls for testing etc.
     * @param edgeToEdgeManager Provides the edge-to-edge state and allows for requests to draw
     *     edge-to-edge.
     * @param browserControlsStateProvider Provides the state of the BrowserControls for Totally
     *     Edge to Edge.
     * @param layoutManagerSupplier The supplier to {@link LayoutManager} for checking the active
     *     layout type.
     * @param fullscreenManager The {@link FullscreenManager} for checking the fullscreen state.
     */
    public EdgeToEdgeControllerImpl(
            Activity activity,
            WindowAndroid windowAndroid,
            NullableObservableSupplier<Tab> tabObservableSupplier,
            @Nullable EdgeToEdgeOSWrapper edgeToEdgeOsWrapper,
            EdgeToEdgeManager edgeToEdgeManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            MonotonicObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager fullscreenManager) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mEdgeToEdgeManager = edgeToEdgeManager;
        mPxToDp = 1.f / mActivity.getResources().getDisplayMetrics().density;
        mDisablePaddingRootView = EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled();

        mEdgeToEdgeOsWrapper =
                edgeToEdgeOsWrapper == null && !mDisablePaddingRootView
                        ? new EdgeToEdgeOSWrapperImpl()
                        : edgeToEdgeOsWrapper;
        mTabSupplierObserver =
                new TabSupplierObserver(tabObservableSupplier) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        onTabSwitched(tab);
                    }
                };
        mTabObserver =
                new TabObserver() {
                    @Override
                    public void onContentChanged(Tab tab) {
                        if (tab == null || tab != mCurrentTab) {
                            return;
                        }
                        boolean topEdgeChanged = updateTopEdgeToEdge();
                        drawToBottomEdge(
                                EdgeToEdgeUtils.isPageOptedIntoBottomEdgeToEdge(tab),
                                /* changedWindowState= */ false);
                        EdgeToEdgeControllerImpl.this.onContentViewScrollingStateChanged(
                                /* scrolling= */ false);
                        if (tab.getWebContents() != null) {
                            updateWebContentsObserver(tab);
                        }
                        // Only retrigger when the top edge-to-edge state changes, to reduce the
                        // number of calls to retriggerOnApplyWindowInsets.
                        if (topEdgeChanged && mInsetObserver != null) {
                            mInsetObserver.retriggerOnApplyWindowInsets();
                        }
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        EdgeToEdgeControllerImpl.this.onContentViewScrollingStateChanged(
                                /* scrolling= */ false);
                    }

                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        EdgeToEdgeControllerImpl.this.onContentViewScrollingStateChanged(
                                /* scrolling= */ false);
                    }

                    @Override
                    public void onContentViewScrollingStateChanged(boolean scrolling) {
                        EdgeToEdgeControllerImpl.this.onContentViewScrollingStateChanged(scrolling);
                    }
                };
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);

        mLayoutManagerSupplier = layoutManagerSupplier;
        mLayoutManagerSupplier.addSyncObserverAndPostIfNonNull(mOnLayoutManagerCallback);
        mLayoutManager = layoutManagerSupplier.get();
        if (mLayoutManager != null) {
            mLayoutManager.addObserver(this);
        }

        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addObserver(this);

        InsetObserver insetObserver = mWindowAndroid.getInsetObserver();
        assert insetObserver != null
                : "The EdgeToEdgeControllerImpl needs access to a valid InsetObserver to listen to"
                        + " the system insets!";
        mInsetObserver = insetObserver;
        mWindowInsetsConsumer = this::handleWindowInsets;
        mInsetObserver.addInsetsConsumer(
                mWindowInsetsConsumer, InsetConsumerSource.EDGE_TO_EDGE_CONTROLLER_IMPL);
        mIsBottomChinEnabled = isBottomChinSupportedByConfiguration(mActivity, mInsetObserver);
        mIsEdgeToEdgeRefactorEnabled = EdgeToEdgeUtils.isEdgeToEdgeRefactorEnabled();

        mEdgeToEdgeStateProvider = mEdgeToEdgeManager.getEdgeToEdgeStateProvider();
        assert mEdgeToEdgeStateProvider != null
                : "The EdgeToEdgeManager needs to provide a valid EdgeToEdgeStateProvider!";
        mEdgeToEdgeToken = mEdgeToEdgeStateProvider.acquireEdgeToEdgeToken();
        assert mEdgeToEdgeToken != TokenHolder.INVALID_TOKEN
                : "The edge-to-edge token should be valid after acquisition!";

        // Any padding to make the content fit the window insets has not yet been applied, so by
        // default, the content is not yet fitting the window insets. The signal should be set to
        // false for now, and updated later if padding gets applied.
        mEdgeToEdgeManager.setContentFitsWindowInsets(false);

        if (mIsEdgeToEdgeRefactorEnabled) {
            mHomepageStateListener =
                    new NtpCustomizationConfigManager.HomepageStateListener() {
                        @Override
                        public void onBackgroundImageChanged(
                                Bitmap originalBitmap,
                                BackgroundImageInfo backgroundImageInfo,
                                boolean fromInitialization,
                                @NtpBackgroundType int oldType,
                                @NtpBackgroundType int newType) {
                            onNtpBackgroundChanged(fromInitialization, oldType, newType);
                        }

                        @Override
                        public void onBackgroundColorChanged(
                                @Nullable NtpThemeColorInfo ntpThemeColorInfo,
                                @ColorInt int backgroundColor,
                                boolean fromInitialization,
                                @NtpBackgroundType int oldType,
                                @NtpBackgroundType int newType) {
                            onNtpBackgroundChanged(fromInitialization, oldType, newType);
                        }

                        @Override
                        public void onBackgroundReset(@NtpBackgroundType int oldType) {
                            onNtpBackgroundReset(oldType);
                        }
                    };
            NtpCustomizationConfigManager manager = NtpCustomizationConfigManager.getInstance();
            manager.addListener(mHomepageStateListener, activity, /* skipNotify= */ false);
        }

        mConsumeTopInset = shouldDrawTopEdgeToEdge(mCurrentTab);
        // retriggerOnApplyWindowInsets to populate all the initial state.
        mIsPageOptedIntoBottomEdgeToEdge =
                EdgeToEdgeUtils.isPageOptedIntoBottomEdgeToEdge(mCurrentTab);
        mInsetObserver.retriggerOnApplyWindowInsets();
    }

    @VisibleForTesting
    static boolean isBottomChinSupportedByConfiguration(
            Activity activity, InsetObserver insetObserver) {
        if (shouldMonitorConfigurationChanges()) {
            return EdgeToEdgeUtils.isEdgeToEdgeBottomChinSupportedByDevice(activity)
                    && EdgeToEdgeUtils.doAllInsetsIndicateGestureNavigation(
                            insetObserver.getLastRawWindowInsets());
        }
        return EdgeToEdgeUtils.isEdgeToEdgeBottomChinSupportedByDevice(activity);
    }

    @VisibleForTesting
    void onTabSwitched(@Nullable Tab tab) {
        if (mCurrentTab != null) mCurrentTab.removeObserver(mTabObserver);
        mCurrentTab = tab;
        if (tab != null) {
            tab.addObserver(mTabObserver);
            if (tab.getWebContents() != null) {
                updateWebContentsObserver(tab);
            }
        }

        boolean topEdgeChanged = updateTopEdgeToEdge();
        drawToBottomEdge(
                EdgeToEdgeUtils.isPageOptedIntoBottomEdgeToEdge(mCurrentTab),
                /* changedWindowState= */ false);
        onContentViewScrollingStateChanged(/* scrolling= */ false);
        // Only retrigger when the top edge-to-edge state changes to
        // avoid unnecessary retriggers.
        if (topEdgeChanged && !mIsTabSwitcherShowing && mInsetObserver != null) {
            mInsetObserver.retriggerOnApplyWindowInsets();
        }
    }

    @Override
    public void addObserver(TopInsetProvider.Observer observer) {
        mTopInsetObservers.addObserver(observer);
        @LayoutType
        int activeLayoutType =
                mLayoutManager != null ? mLayoutManager.getActiveLayoutType() : LayoutType.NONE;
        observer.onToEdgeChange(mSystemInsets.top, isDrawingToTopEdge(), activeLayoutType);
    }

    @Override
    public void removeObserver(TopInsetProvider.Observer observer) {
        mTopInsetObservers.removeObserver(observer);
    }

    @Override
    public boolean isDrawingToTopEdge() {
        // TODO(crbug.com/498302496): When top edge-to-edge expands to web pages, update this to
        // also check top edge-to-edge page opt-in.
        assert !mConsumeTopInset || mIsEdgeToEdgeRefactorEnabled
                : "Top inset should not be consumed when edge-to-edge top inset is disabled";
        return mConsumeTopInset;
    }

    private boolean shouldDrawTopEdgeToEdge(@Nullable Tab tab) {
        if (!EdgeToEdgeUtils.isEdgelessTopInsetSupported(mActivity) || mStatusIndicatorVisible) {
            return false;
        }
        if (EdgeToEdgeUtils.tabSupportsTopEdgeToEdge(tab)) {
            return true;
        }
        // Preserves TopInsetCoordinator parity: During initial tab creation with an NTP URL,
        // tab.isNativePage() is temporarily false before the NewTabPage object is instantiated.
        // Check if the tab is a regular NTP with a customized background so insets are consumed
        // immediately during tab creation without waiting for the NativePage to initialize.
        if (EdgeToEdgeUtils.isRegularNtp(tab)) {
            return NtpCustomizationConfigManager.getInstance().getBackgroundType()
                    != NtpBackgroundType.DEFAULT;
        }
        return false;
    }

    /**
     * Updates whether the system is drawing edge-to-edge on the top based on the current tab, and
     * adjusts the root view edge paddings if the top edge-to-edge state changed.
     *
     * @return Whether the top edge-to-edge state changed.
     */
    private boolean updateTopEdgeToEdge() {
        boolean wasDrawingToTopEdge = isDrawingToTopEdge();
        mConsumeTopInset = shouldDrawTopEdgeToEdge(mCurrentTab);
        boolean changedTopEdge = wasDrawingToTopEdge != isDrawingToTopEdge();
        if (changedTopEdge) {
            adjustEdgePaddings();
        }
        return changedTopEdge;
    }

    private void notifyTopInsetObservers() {
        @LayoutType
        int activeLayoutType =
                mLayoutManager != null ? mLayoutManager.getActiveLayoutType() : LayoutType.NONE;
        // Skip notifying top observers when in the Hub with a null tab (crbug.com/491888405) so
        // the toolbar does not lose its top padding when closing the last tab in the Hub.
        boolean shouldNotifyTopObservers =
                mCurrentTab != null || activeLayoutType != LayoutType.HUB;
        if (shouldNotifyTopObservers) {
            for (var observer : mTopInsetObservers) {
                observer.onToEdgeChange(mSystemInsets.top, isDrawingToTopEdge(), activeLayoutType);
            }
        }
    }

    @Override
    public void setStatusIndicatorVisible(boolean visible) {
        if (mStatusIndicatorVisible == visible) return;
        mStatusIndicatorVisible = visible;
        updateTopEdgeToEdge();
        if (mInsetObserver != null) {
            mInsetObserver.retriggerOnApplyWindowInsets();
        }
    }

    @VisibleForTesting
    void onNtpBackgroundChanged(
            boolean fromInitialization,
            @NtpBackgroundType int oldType,
            @NtpBackgroundType int newType) {
        if (oldType == newType) return;

        boolean shouldRefreshWindowInsets = oldType == NtpBackgroundType.DEFAULT;
        if (fromInitialization || !shouldRefreshWindowInsets) return;

        updateTopEdgeToEdge();
        if (mInsetObserver != null) {
            mInsetObserver.retriggerOnApplyWindowInsets();
        }
    }

    @VisibleForTesting
    void onNtpBackgroundReset(@NtpBackgroundType int oldType) {
        if (oldType == NtpBackgroundType.DEFAULT) return;

        updateTopEdgeToEdge();
        if (mInsetObserver != null) {
            mInsetObserver.retriggerOnApplyWindowInsets();
        }
    }

    @Override
    public void registerAdjuster(EdgeToEdgePadAdjuster adjuster) {
        mPadAdjusters.addObserver(adjuster);
        boolean shouldPad = shouldPadAdjusters();
        // TODO(crbug.com/498302496): Support top pad adjusters (e.g. for top-aligned overlays and
        // dialogs) when unifying top and bottom E2E pad adjusters.
        adjuster.overrideBottomInset(shouldPad ? mSystemInsets.bottom : 0);
    }

    @Override
    public void unregisterAdjuster(EdgeToEdgePadAdjuster adjuster) {
        mPadAdjusters.removeObserver(adjuster);
    }

    @Override
    public void registerObserver(ChangeObserver changeObserver) {
        mBottomEdgeChangeObservers.addObserver(changeObserver);
    }

    @Override
    public void unregisterObserver(ChangeObserver changeObserver) {
        mBottomEdgeChangeObservers.removeObserver(changeObserver);
    }

    @Override
    public int getBottomInset() {
        return mIsDrawingToBottomEdge ? (int) Math.ceil(mSystemInsets.bottom * mPxToDp) : 0;
    }

    @Override
    public int getBottomInsetPx() {
        return mIsDrawingToBottomEdge ? mSystemInsets.bottom : 0;
    }

    @Override
    public int getSystemBottomInsetPx() {
        return mSystemInsets.bottom;
    }

    @Override
    public boolean isDrawingToEdge() {
        return mIsDrawingToBottomEdge;
    }

    @Override
    public boolean isPageOptedIntoEdgeToEdge() {
        return mIsPageOptedIntoBottomEdgeToEdge;
    }

    // BrowserControlsStateProvider.Observer

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
        updateBrowserControlsVisibility(
                mBottomControlsHeight > 0 && bottomOffset < mBottomControlsHeight);
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        // The bottom controls are shown / hidden from the user by changing the height, rather than
        // changing view visibility.
        mBottomControlsHeight = bottomControlsHeight;
        updateBrowserControlsVisibility(bottomControlsHeight > 0);
        adjustEdgePaddings();
        pushSafeAreaInsetUpdate();
    }

    @VisibleForTesting
    void onContentViewScrollingStateChanged(boolean scrolling) {
        if (mContentViewScrolling == scrolling) return;
        mContentViewScrolling = scrolling;
        if (!scrolling && ChromeFeatureList.sBottomControlsJankImprovement.isEnabled()) {
            if (mPadAdjustersUpdatePending) {
                updatePadAdjusters();
            }
        }
    }

    // LayoutStateProvider.LayoutStateObserver

    @Override
    public void onStartedShowing(int layoutType) {
        drawToBottomEdge(mIsPageOptedIntoBottomEdgeToEdge, false);
    }

    @Override
    public void onFinishedShowing(int layoutType) {
        if (layoutType == LayoutType.HUB) {
            mIsTabSwitcherShowing = true;
        } else {
            mIsTabSwitcherShowing = false;
        }
    }

    @Override
    public void onFinishedHiding(int layoutType) {
        if (layoutType == LayoutType.HUB) {
            // Retrigger window insets when exiting Hub to ensure insets are accurately applied.
            // If performance issues arise from retriggering on every Hub exit, consider tracking
            // whether the transition is specifically to a tab with top edge-to-edge support.
            if (mInsetObserver != null) {
                mInsetObserver.retriggerOnApplyWindowInsets();
            }
        }
    }

    // FullscreenManager.Observer
    @Override
    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
        drawToBottomEdge(mIsPageOptedIntoBottomEdgeToEdge, /* changedWindowState= */ true);
    }

    @Override
    public void onExitFullscreen(Tab tab) {
        drawToBottomEdge(mIsPageOptedIntoBottomEdgeToEdge, /* changedWindowState= */ true);
    }

    private View getContentView() {
        return mActivity.findViewById(ROOT_UI_VIEW_ID);
    }

    private void updateBrowserControlsVisibility(boolean visible) {
        if (mBottomControlsAreVisible == visible) {
            return;
        }
        mBottomControlsAreVisible = visible;
        if (mContentViewScrolling && ChromeFeatureList.sBottomControlsJankImprovement.isEnabled()) {
            mPadAdjustersUpdatePending = true;
            return;
        }
        updatePadAdjusters();
    }

    /**
     * Updates our private WebContentsObserver member to point to the given Tab's WebContents.
     * Destroys any previous member.
     *
     * @param tab The {@link Tab} whose {@link WebContents} we want to observe.
     */
    private void updateWebContentsObserver(Tab tab) {
        if (mWebContentsObserver != null) mWebContentsObserver.observe(null);
        mWebContentsObserver =
                new WebContentsObserver(tab.getWebContents()) {
                    @Override
                    public void viewportFitChanged(@WebContentsObserver.ViewportFitType int value) {
                        drawToBottomEdge(
                                EdgeToEdgeUtils.isPageOptedIntoBottomEdgeToEdge(mCurrentTab, value),
                                /* changedWindowState= */ false);
                    }

                    @Override
                    public void safeAreaConstraintChanged(boolean hasConstraint) {
                        if (mHasSafeAreaConstraint == hasConstraint) {
                            return;
                        }

                        mHasSafeAreaConstraint = hasConstraint;
                        for (var observer : mBottomEdgeChangeObservers) {
                            observer.onSafeAreaConstraintChanged(mHasSafeAreaConstraint);
                        }
                    }
                };
    }

    private void updateLayoutStateProvider(
            @Nullable LayoutManager newValue, @Nullable LayoutManager oldValue) {
        if (oldValue != null) {
            oldValue.removeObserver(this);
        }
        if (newValue != null) {
            newValue.addObserver(this);
        }
        mLayoutManager = newValue;
        drawToBottomEdge(
                EdgeToEdgeUtils.isPageOptedIntoBottomEdgeToEdge(mCurrentTab),
                /* changedWindowState= */ false);
    }

    /**
     * Conditionally draws the given View ToEdge or ToNormal on the bottom based on the {@code
     * pageOptedIntoBottomEdgeToEdge} param.
     *
     * @param pageOptedIntoBottomEdgeToEdge Whether the page is opted into bottom edge-to-edge.
     * @param changedWindowState Whether this method is called due to window state changed (e.g.
     *     windowInsets updated, window goes into fullscreen mode).
     */
    @VisibleForTesting
    void drawToBottomEdge(boolean pageOptedIntoBottomEdgeToEdge, boolean changedWindowState) {
        final boolean isChinEnabled =
                isBottomChinSupportedByConfiguration(mActivity, mInsetObserver);

        if (!isChinEnabled && !mIsEdgeToEdgeRefactorEnabled) {
            EdgeToEdgeUtils.recordDrawToEdgeInUnsupportedConfig(changedWindowState);
        }

        // Exit early if there is a tappable navbar (3-button) as bottom edge to edge should not
        // function when 3-button nav is enabled, unless top edge to edge is enabled.
        if (!shouldMonitorConfigurationChanges()
                && EdgeToEdgeUtils.hasTappableNavigationBar(mActivity.getWindow())
                && !mIsEdgeToEdgeRefactorEnabled) {
            return;
        }

        @LayoutType
        int currentLayoutType =
                mLayoutManager != null ? mLayoutManager.getActiveLayoutType() : LayoutType.NONE;
        boolean shouldDrawToBottomEdge =
                EdgeToEdgeUtils.shouldDrawToBottomEdge(
                        pageOptedIntoBottomEdgeToEdge, currentLayoutType, mSystemInsets.bottom);
        shouldDrawToBottomEdge &= isChinEnabled;
        pageOptedIntoBottomEdgeToEdge &= isChinEnabled;
        // Refresh the mHasSafeAreaConstraint to ensure the boolean stays fresh (e.g. when
        // #drawToBottomEdge is called due to tab switching)
        boolean hasSafeAreaConstraint = EdgeToEdgeUtils.hasSafeAreaConstraintForTab(mCurrentTab);

        boolean changedPageOptedIn =
                pageOptedIntoBottomEdgeToEdge != mIsPageOptedIntoBottomEdgeToEdge;
        boolean changedDrawToBottomEdge = shouldDrawToBottomEdge != mIsDrawingToBottomEdge;
        boolean changedSafeAreaConstraint = mHasSafeAreaConstraint != hasSafeAreaConstraint;
        mIsPageOptedIntoBottomEdgeToEdge = pageOptedIntoBottomEdgeToEdge;
        mIsDrawingToBottomEdge = shouldDrawToBottomEdge;
        mHasSafeAreaConstraint = hasSafeAreaConstraint;

        if (changedPageOptedIn) {
            Log.v(
                    TAG,
                    "Switching %s",
                    (mIsPageOptedIntoBottomEdgeToEdge
                            ? "Opted into EdgeToEdge"
                            : "Not opted into EdgeToEdge"));
        }

        if (changedDrawToBottomEdge) {
            Log.v(TAG, "Switching %s", (mIsDrawingToBottomEdge ? "ToEdge" : "ToNormal"));
        }

        if (changedPageOptedIn || changedDrawToBottomEdge || changedWindowState) {
            adjustEdgePaddings();
            pushSafeAreaInsetUpdate();
            updatePadAdjusters();

            for (var observer : mBottomEdgeChangeObservers) {
                observer.onToEdgeChange(
                        mSystemInsets.bottom,
                        mIsDrawingToBottomEdge,
                        mIsPageOptedIntoBottomEdgeToEdge);
            }
        }

        if (changedSafeAreaConstraint) {
            for (var observer : mBottomEdgeChangeObservers) {
                observer.onSafeAreaConstraintChanged(mHasSafeAreaConstraint);
            }
        }
    }

    @VisibleForTesting
    WindowInsetsCompat handleWindowInsets(View rootView, WindowInsetsCompat windowInsets) {
        boolean changedWindowState = false;
        boolean switchedToUnsupportedConfig = false;
        if (mIsBottomChinEnabled
                != isBottomChinSupportedByConfiguration(mActivity, mInsetObserver)) {
            Log.v(
                    TAG,
                    "Switching supported configuration from %s",
                    (mIsBottomChinEnabled
                            ? "supported to unsupported"
                            : "unsupported to supported"));
            switchedToUnsupportedConfig = mIsBottomChinEnabled;
            mIsBottomChinEnabled = isBottomChinSupportedByConfiguration(mActivity, mInsetObserver);
            EdgeToEdgeUtils.recordSupportedConfigurationSwitch(mIsBottomChinEnabled);
            if (mCurrentTab != null) {
                mIsPageOptedIntoBottomEdgeToEdge =
                        EdgeToEdgeUtils.isPageOptedIntoBottomEdgeToEdge(mCurrentTab)
                                && mIsBottomChinEnabled;
            }
            changedWindowState = true;
        }
        if (mIsBottomChinEnabled) {
            EdgeToEdgeUtils.verifyInsetsInSupportedConfiguration(windowInsets);
        }

        // Exit early if there is a tappable navbar (3-button) as bottom edge to edge should not
        // function when 3-button nav is enabled, unless top edge to edge is enabled.
        if (!shouldMonitorConfigurationChanges()
                && !mIsEdgeToEdgeRefactorEnabled
                && EdgeToEdgeUtils.hasTappableNavigationBar(mActivity.getWindow())) {
            return windowInsets;
        }

        Insets originalSystemInsets = mSystemInsets;
        Insets newInsets =
                getSystemInsets(windowInsets, mInsetObserver.hasSeenNonZeroNavigationBarInsets());
        Insets newKeyboardInsets = windowInsets.getInsets(WindowInsetsCompat.Type.ime());

        if (updateVisibilityRects(rootView)
                || !newInsets.equals(originalSystemInsets)
                || !newKeyboardInsets.equals(mKeyboardInsets)) {
            mSystemInsets = newInsets;
            mKeyboardInsets = newKeyboardInsets;

            // When a foldable goes to/from tablet mode we must reassess.
            // TODO(https://crbug.com/325356134) Find a cleaner check and remedy.
            mIsPageOptedIntoBottomEdgeToEdge =
                    mIsPageOptedIntoBottomEdgeToEdge
                            && isBottomChinSupportedByConfiguration(mActivity, mInsetObserver);

            changedWindowState = true;
        }

        if (mIsEdgeToEdgeRefactorEnabled) {
            updateTopEdgeToEdge();
            notifyTopInsetObservers();
        }

        // Note that we cannot call #drawToBottomEdge earlier since we need the system
        // insets.
        if (changedWindowState) {
            drawToBottomEdge(mIsPageOptedIntoBottomEdgeToEdge, /* changedWindowState= */ true);
        }
        // Signal: When configuration is changed, did we pad the system correctly.
        if (switchedToUnsupportedConfig) {
            EdgeToEdgeUtils.recordConfigurationSwitchScenario(
                    originalSystemInsets, newInsets, mAppliedContentViewPadding);
        }

        // TODO(crbug.com/498302496): In the unified top scalp architecture, top window insets will
        // be consumed at the root view level and managed by top controls.
        // Consume top insets when in persistent fullscreen or for top e2e.
        boolean consumeTopInsets =
                mIsEdgeToEdgeRefactorEnabled
                        ? ((mFullscreenManager != null
                                        && mFullscreenManager.getPersistentFullscreenMode())
                                || isDrawingToTopEdge())
                        : (mAppliedContentViewPadding.top == 0);
        boolean consumeBottomInsets = mAppliedContentViewPadding.bottom == 0;
        return consumeWindowInsets(windowInsets, consumeTopInsets, consumeBottomInsets);
    }

    private boolean updateVisibilityRects(View rootView) {
        Rect windowVisibleRect = new Rect();
        rootView.getWindowVisibleDisplayFrame(windowVisibleRect);

        Rect contentVisibleRect = new Rect();
        View contentView = getContentView();
        if (contentView != null) {
            contentView.getGlobalVisibleRect(contentVisibleRect);
            int[] locationOnScreen = new int[2];
            rootView.getLocationOnScreen(locationOnScreen);
            contentVisibleRect.offset(locationOnScreen[0], locationOnScreen[1]);
        }

        if (windowVisibleRect.equals(mCachedWindowVisibleRect)
                && contentVisibleRect.equals(mCachedContentVisibleRect)) {
            return false;
        }
        mCachedWindowVisibleRect.set(windowVisibleRect);
        mCachedContentVisibleRect.set(contentVisibleRect);
        return true;
    }

    /**
     * Returns whether the IME insets are taller than the navigation bar (e.g. a soft keyboard is
     * showing). IME sessions without a soft keyboard (such as stylus handwriting) report an IME
     * inset equal to the navigation bar.
     */
    private boolean isKeyboardInsetTallerThanNavBar() {
        return mKeyboardInsets != null
                && mKeyboardInsets.bottom > (mSystemInsets != null ? mSystemInsets.bottom : 0);
    }

    /**
     * The {@link EdgeToEdgePadAdjuster}s should only be padded with an extra bottom inset if the
     * activity is currently in edge-to-edge, and if the adjusters aren't already positioned above
     * the system insets due to the keyboard or the bottom controls being visible.
     */
    private boolean shouldPadAdjusters() {
        // Never pad the adjusters if the keyboard is visible above the navigation bar.
        if (isKeyboardInsetTallerThanNavBar()) return false;

        // Never pad the adjusters if the bottom controls are visible. Except in the tab switcher.
        @LayoutType
        int currentLayoutType =
                mLayoutManager != null ? mLayoutManager.getActiveLayoutType() : LayoutType.NONE;
        if (mBottomControlsAreVisible && currentLayoutType != LayoutType.HUB) return false;

        // Pad the adjusters if drawing to bottom edge.
        return mIsDrawingToBottomEdge;
    }

    private void updatePadAdjusters() {
        mPadAdjustersUpdatePending = false;
        boolean shouldPad = shouldPadAdjusters();
        // TODO(crbug.com/498302496): Update top pad adjusters with mSystemInsets.top when unified
        // pad adjusters are added.
        for (var adjuster : mPadAdjusters) {
            adjuster.overrideBottomInset(shouldPad ? mSystemInsets.bottom : 0);
        }
    }

    /**
     * Adjusts whether the given view draws ToEdge or ToNormal. The ability to draw under System
     * Bars should have already been set. This method only sets the padding of the view and
     * transparency of the Nav Bar, etc.
     */
    private void adjustEdgePaddings() {
        // TODO(crbug.com/377959835): Move padding logic to the EdgeToEdgeManager, to be triggered
        //  by calls to this #setContentFitsWindow() method.
        // Content should fit within the window insets if the activity is not drawing edge-to-edge.
        if (!EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled()) {
            mEdgeToEdgeManager.setContentFitsWindowInsets(!mIsDrawingToBottomEdge);
        }

        View contentView = getContentView();
        assert contentView != null : "Root view for Edge To Edge not found!";

        // Adjust the top and bottom padding to reflect whether ToEdge or ToNormal for the Status
        // Bar and Gesture Nav Bar. All the other edges need to be padded to prevent drawing under
        // an edge that we don't want drawn ToEdge.
        int topPadding = isDrawingToTopEdge() ? 0 : mSystemInsets.top;
        int bottomPadding = mIsDrawingToBottomEdge ? 0 : mSystemInsets.bottom;
        // If the keyboard is taller than the navigation bar, pad content to account for it.
        if (isKeyboardInsetTallerThanNavBar()) {
            assert mKeyboardInsets != null;
            bottomPadding = mKeyboardInsets.bottom;
        }

        // In fullscreen mode, there are cases the content isn't being drawn under the system
        // bar (e.g. during multi-window mode). In this case, adjust the padding based on the
        // visibility rects. See https://crbug.com/359659885
        if (mFullscreenManager.getPersistentFullscreenMode()) {
            topPadding = 0;
            bottomPadding = 0;
        }

        // Use Insets to store the paddings as it is immutable.
        Insets newPaddings =
                Insets.of(mSystemInsets.left, topPadding, mSystemInsets.right, bottomPadding);
        boolean paddingChanged = !newPaddings.equals(mAppliedContentViewPadding);
        mAppliedContentViewPadding = newPaddings;
        if (paddingChanged && !mDisablePaddingRootView && mEdgeToEdgeOsWrapper != null) {
            mEdgeToEdgeOsWrapper.setPadding(
                    contentView,
                    newPaddings.left,
                    newPaddings.top,
                    newPaddings.right,
                    newPaddings.bottom);
        }
    }

    private void pushSafeAreaInsetUpdate() {
        // In fullscreen mode, we should never needed to add additional area to the bottom insets
        // since nav bar will be hidden. This is another workaround that on some Android versions,
        // during split screen mode, bottom insets are counted as part of the Chrome window even
        // when Chrome does not draw into the system bar region. See https://crbug.com/359659885.
        boolean hasBottomSafeArea =
                (mIsDrawingToBottomEdge && !mFullscreenManager.getPersistentFullscreenMode());

        // When the content view is padded (e.g. when keyboard is showing), we should not count the
        // padding as part of the bottom safe area.
        int safeAreaInsets = Math.max(mSystemInsets.bottom - mAppliedContentViewPadding.bottom, 0);

        int bottomInsetOnSafeArea = hasBottomSafeArea ? safeAreaInsets : 0;
        mInsetObserver.updateBottomInsetForEdgeToEdge(bottomInsetOnSafeArea);

        // TODO(crbug.com/498302496): When top edge-to-edge expands to web pages, push top safe area
        // insets to InsetObserver (e.g. updateTopInsetForEdgeToEdge) for Blink CSS
        // env(safe-area-inset-top).
    }

    @SuppressWarnings("NullAway")
    @CallSuper
    @Override
    public void destroy() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.observe(null);
            mWebContentsObserver = null;
        }
        if (mCurrentTab != null) mCurrentTab.removeObserver(mTabObserver);
        mTabSupplierObserver.destroy();
        if (mInsetObserver != null) {
            assumeNonNull(mWindowInsetsConsumer);
            mInsetObserver.removeInsetsConsumer(mWindowInsetsConsumer);
            mInsetObserver = null;
        }
        if (mBrowserControlsStateProvider != null) {
            mBrowserControlsStateProvider.removeObserver(this);
        }
        if (mOnLayoutManagerCallback != null) {
            mLayoutManagerSupplier.removeObserver(mOnLayoutManagerCallback);
        }
        if (mLayoutManager != null) {
            mLayoutManager.removeObserver(this);
            mLayoutManager = null;
        }
        if (mFullscreenManager != null) {
            mFullscreenManager.removeObserver(this);
        }
        if (mHomepageStateListener != null) {
            NtpCustomizationConfigManager.getInstance().removeListener(mHomepageStateListener);
            mHomepageStateListener = null;
        }
        mTopInsetObservers.clear();
        mEdgeToEdgeStateProvider.releaseEdgeToEdgeToken(mEdgeToEdgeToken);
    }

    @VisibleForTesting
    @Nullable WebContentsObserver getWebContentsObserver() {
        return mWebContentsObserver;
    }

    private static boolean shouldMonitorConfigurationChanges() {
        return ChromeFeatureList.sEdgeToEdgeMonitorConfigurations.isEnabled();
    }

    TabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    public void setIsOptedIntoBottomEdgeToEdgeForTesting(boolean toEdge) {
        mIsPageOptedIntoBottomEdgeToEdge = toEdge;
    }

    public void setIsDrawingToBottomEdgeForTesting(boolean toEdge) {
        mIsDrawingToBottomEdge = toEdge;
    }

    public @Nullable ChangeObserver getAnyChangeObserverForTesting() {
        return mBottomEdgeChangeObservers.isEmpty()
                ? null
                : mBottomEdgeChangeObservers.iterator().next();
    }

    public void setConsumeTopInsetForTesting(boolean consumeTopInset) {
        mConsumeTopInset = consumeTopInset;
    }

    void setSystemInsetsForTesting(Insets systemInsetsForTesting) {
        mSystemInsets = systemInsetsForTesting;
    }

    void setKeyboardInsetsForTesting(Insets keyboardInsetsForTesting) {
        mKeyboardInsets = keyboardInsetsForTesting;
    }

    public boolean getHasSafeAreaConstraintForTesting() {
        return mHasSafeAreaConstraint;
    }

    public Insets getAppliedContentViewPaddingForTesting() {
        return mAppliedContentViewPadding;
    }

    @VisibleForTesting
    static WindowInsetsCompat consumeWindowInsets(
            WindowInsetsCompat windowInsets,
            boolean consumeTopInsets,
            boolean consumeBottomInsets) {
        if (!consumeTopInsets && !consumeBottomInsets) {
            return windowInsets;
        }
        var builder = new WindowInsetsCompat.Builder(windowInsets);

        builder.setInsets(
                WindowInsetsCompat.Type.statusBars(),
                consumeTopInsets
                        ? Insets.NONE
                        : windowInsets.getInsets(WindowInsetsCompat.Type.statusBars()));
        builder.setInsets(
                WindowInsetsCompat.Type.captionBar(),
                consumeTopInsets
                        ? Insets.NONE
                        : windowInsets.getInsets(WindowInsetsCompat.Type.captionBar()));

        // TODO(crbug.com/498302496): Only the top display cutout is consumed for now.
        // Support for drawing into display cutouts on the side with pillarboxing will be
        // added in future iterations.
        Insets displayCutout = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout());
        if (displayCutout.top > 0) {
            builder.setInsets(
                    WindowInsetsCompat.Type.displayCutout(),
                    Insets.of(
                            displayCutout.left,
                            consumeTopInsets ? 0 : displayCutout.top,
                            displayCutout.right,
                            displayCutout.bottom));
        }

        builder.setInsets(
                WindowInsetsCompat.Type.navigationBars(),
                consumeBottomInsets
                        ? Insets.NONE
                        : windowInsets.getInsets(WindowInsetsCompat.Type.navigationBars()));
        builder.setInsets(
                WindowInsetsCompat.Type.tappableElement(),
                consumeBottomInsets
                        ? Insets.NONE
                        : windowInsets.getInsets(WindowInsetsCompat.Type.tappableElement()));
        builder.setInsets(
                WindowInsetsCompat.Type.ime(),
                consumeBottomInsets
                        ? Insets.NONE
                        : windowInsets.getInsets(WindowInsetsCompat.Type.ime()));

        Insets mandatorySystemGestures =
                windowInsets.getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        builder.setInsets(
                WindowInsetsCompat.Type.mandatorySystemGestures(),
                Insets.of(
                        mandatorySystemGestures.left,
                        mandatorySystemGestures.top,
                        mandatorySystemGestures.right,
                        consumeBottomInsets ? 0 : mandatorySystemGestures.bottom));

        return builder.build();
    }

    private static Insets getSystemInsets(
            WindowInsetsCompat windowInsets, boolean hasSeenNonZeroNavigationBarInsets) {
        Insets systemBarInsets = windowInsets.getInsets(systemBars() + tappableElement());

        if (!EdgeToEdgeUtils.isUseBackupNavbarInsetsEnabled()) return systemBarInsets;

        if (systemBarInsets.left == 0
                && systemBarInsets.right == 0
                && systemBarInsets.bottom == 0) {
            @Nullable Insets backupNavbarInsets =
                    EdgeToEdgeManager.getBackupNavbarInsets(
                            hasSeenNonZeroNavigationBarInsets,
                            windowInsets,
                            BackupNavbarInsetsCallSite.EDGE_TO_EDGE_CONTROLLER,
                            EdgeToEdgeFieldTrialImpl.getBackupNavbarInsetsOverrides(),
                            ChromeFeatureList.sEdgeToEdgeUseBackupNavbarInsetsUseGestures
                                    .getValue());
            // If applicable, apply backup navbar insets to the left, right, and bottom (not the
            // top, as that's always the status bar).
            if (backupNavbarInsets != null) {
                systemBarInsets =
                        Insets.of(
                                backupNavbarInsets.left,
                                systemBarInsets.top,
                                backupNavbarInsets.right,
                                backupNavbarInsets.bottom);
            }
        }
        return systemBarInsets;
    }
}
