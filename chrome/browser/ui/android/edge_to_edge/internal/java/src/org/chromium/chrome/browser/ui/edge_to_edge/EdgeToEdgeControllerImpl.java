// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static androidx.core.view.WindowInsetsCompat.Type.systemBars;
import static androidx.core.view.WindowInsetsCompat.Type.tappableElement;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
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

/**
 * Controls use of the Android Edge To Edge feature that allows an App to draw benieth the Status
 * and Navigation Bars. For Chrome, we intentend to sometimes draw under the Nav Bar but not the
 * Status Bar.
 */
@NullMarked
@RequiresApi(VERSION_CODES.R)
public class EdgeToEdgeControllerImpl
        implements EdgeToEdgeController,
                BrowserControlsStateProvider.Observer,
                LayoutStateProvider.LayoutStateObserver,
                FullscreenManager.Observer {
    private static final String TAG = "E2E_ControllerImpl";
    private static final String DRAW_TO_EDGE_UNSUPPORTED_CONFIG_HISTOGRAM =
            "Android.EdgeToEdge.DrawToEdgeInUnsupportedConfiguration";
    private static final String SUPPORTED_CONFIGURATION_SWITCH_HISTOGRAM =
            "Android.EdgeToEdge.SupportedConfigurationSwitch2";
    private static final String CONFIGURATION_SWITCH_OUTCOME_HISTOGRAM =
            "Android.EdgeToEdge.Debugging.ConfigurationSwitchOutcome";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        SupportedConfigurationSwitch.FROM_SUPPORTED_TO_UNSUPPORTED,
        SupportedConfigurationSwitch.FROM_UNSUPPORTED_TO_SUPPORTED,
        SupportedConfigurationSwitch.NUM_ENTRIES
    })
    @interface SupportedConfigurationSwitch {
        int FROM_SUPPORTED_TO_UNSUPPORTED = 0;
        int FROM_UNSUPPORTED_TO_SUPPORTED = 1;
        int NUM_ENTRIES = 2;
    }

    /** When configuration changes from supported to unsupported, what's the outcome */
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        ConfigurationSwitchOutcome.ADD_PADDING_NEW_INSETS,
        ConfigurationSwitchOutcome.ADD_PADDING_ORIGINAL_INSETS,
        ConfigurationSwitchOutcome.ERROR_ADD_PADDING_BOTH_INSETS_EMPTY,
        ConfigurationSwitchOutcome.NO_PADDING_BOTH_INSETS_EMPTY,
        ConfigurationSwitchOutcome.NO_PADDING_NO_NEW_INSETS,
        ConfigurationSwitchOutcome.ERROR_NO_PADDING_WITH_NEW_INSETS,
        ConfigurationSwitchOutcome.NUM_ENTRIES
    })
    public @interface ConfigurationSwitchOutcome {

        // Correct cases
        int ADD_PADDING_ORIGINAL_INSETS = 0;
        int ADD_PADDING_NEW_INSETS = 1;
        // Error case / impossible case
        int ERROR_ADD_PADDING_BOTH_INSETS_EMPTY = 2;
        int NO_PADDING_BOTH_INSETS_EMPTY = 3;
        int NO_PADDING_NO_NEW_INSETS = 4;
        // Error case / impossible case
        int ERROR_NO_PADDING_WITH_NEW_INSETS = 5;

        int NUM_ENTRIES = 6;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        SupportedConfigurationStrangeInsetsState.TAPPABLE_ELEMENT_NOT_GESTURE_NAV,
        SupportedConfigurationStrangeInsetsState.NO_TAPPABLE_ELEMENT_NOT_GESTURE_NAV,
        SupportedConfigurationStrangeInsetsState.ERROR_TAPPABLE_ELEMENT_GESTURE_NAV,
        SupportedConfigurationStrangeInsetsState.NUM_ENTRIES
    })
    @interface SupportedConfigurationStrangeInsetsState {
        int TAPPABLE_ELEMENT_NOT_GESTURE_NAV = 0;
        int NO_TAPPABLE_ELEMENT_NOT_GESTURE_NAV = 1;
        int ERROR_TAPPABLE_ELEMENT_GESTURE_NAV = 2;
        int NUM_ENTRIES = 2;
    }

    /** The outermost view in our view hierarchy that is identified with a resource ID. */
    private static final int ROOT_UI_VIEW_ID = android.R.id.content;

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final TabSupplierObserver mTabSupplierObserver;
    private final ObserverList<EdgeToEdgePadAdjuster> mPadAdjusters = new ObserverList<>();
    private final ObserverList<ChangeObserver> mEdgeChangeObservers = new ObserverList<>();
    private final TabObserver mTabObserver;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ObservableSupplier<LayoutManager> mLayoutManagerSupplier;
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
     * This could be due to the current page being opted into edge-to-edge, or a partial
     * edge-to-edge with the bottom chin present.
     */
    private boolean mIsDrawingToEdge;

    /**
     * Whether the edge-to-edge feature is enabled and the current tab content is showing
     * edge-to-edge. This could be from the web content being opted in, or from the tab showing a
     * native page that supports edge-to-edge.
     */
    private boolean mIsPageOptedIntoEdgeToEdge;

    /**
     * Whether the page should constrain the safe area, which requires the page to be retained
     * within the safe area region. This essentially opts the page out of edge-to-edge, regardless
     * of other flags and values (e.g. |mIsPageOptedIntoEdgeToEdge|)
     */
    private boolean mHasSafeAreaConstraint;

    private InsetObserver mInsetObserver;
    private Insets mSystemInsets = Insets.NONE;
    private Insets mAppliedContentViewPadding = Insets.NONE;
    private @Nullable Insets mKeyboardInsets;
    private final @Nullable WindowInsetsConsumer mWindowInsetsConsumer;
    private boolean mBottomControlsAreVisible;
    private int mBottomControlsHeight;

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
            ObservableSupplier<@Nullable Tab> tabObservableSupplier,
            @Nullable EdgeToEdgeOSWrapper edgeToEdgeOsWrapper,
            EdgeToEdgeManager edgeToEdgeManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
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
                new EmptyTabObserver() {
                    @Override
                    public void onContentChanged(Tab tab) {
                        assert tab.getWebContents() != null
                                : "onContentChanged called on tab w/o WebContents: "
                                        + tab.getTitle();
                        drawToEdge(
                                EdgeToEdgeUtils.isPageOptedIntoEdgeToEdge(mCurrentTab),
                                /* changedWindowState= */ false);
                        updateWebContentsObserver(tab);
                    }
                };
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);

        mLayoutManagerSupplier = layoutManagerSupplier;
        mLayoutManagerSupplier.addObserver(mOnLayoutManagerCallback);
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
        mIsBottomChinEnabled = isSupportedByConfiguration(mActivity, mInsetObserver);

        mEdgeToEdgeStateProvider = mEdgeToEdgeManager.getEdgeToEdgeStateProvider();
        mEdgeToEdgeToken = mEdgeToEdgeStateProvider.acquireSetDecorFitsSystemWindowToken();

        // Any padding to make the content fit the window insets has not yet been applied, so by
        // default, the content is not yet fitting the window insets. The signal should be set to
        // false for now, and updated later if padding gets applied.
        mEdgeToEdgeManager.setContentFitsWindowInsets(false);

        // retriggerOnApplyWindowInsets to populate all the initial state.
        mIsPageOptedIntoEdgeToEdge = EdgeToEdgeUtils.isPageOptedIntoEdgeToEdge(mCurrentTab);
        mInsetObserver.retriggerOnApplyWindowInsets();
    }

    @VisibleForTesting
    static boolean isSupportedByConfiguration(Activity activity, InsetObserver insetObserver) {
        if (shouldMonitorConfigurationChanges()) {
            return EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(activity)
                    && EdgeToEdgeUtils.doAllInsetsIndicateGestureNavigation(
                            insetObserver.getLastRawWindowInsets());
        }
        return EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(activity);
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

        drawToEdge(
                EdgeToEdgeUtils.isPageOptedIntoEdgeToEdge(mCurrentTab),
                /* changedWindowState= */ false);
    }

    @Override
    public void registerAdjuster(EdgeToEdgePadAdjuster adjuster) {
        mPadAdjusters.addObserver(adjuster);
        boolean shouldPad = shouldPadAdjusters();
        adjuster.overrideBottomInset(shouldPad ? mSystemInsets.bottom : 0);
    }

    @Override
    public void unregisterAdjuster(EdgeToEdgePadAdjuster adjuster) {
        mPadAdjusters.removeObserver(adjuster);
    }

    @Override
    public void registerObserver(ChangeObserver changeObserver) {
        mEdgeChangeObservers.addObserver(changeObserver);
    }

    @Override
    public void unregisterObserver(ChangeObserver changeObserver) {
        mEdgeChangeObservers.removeObserver(changeObserver);
    }

    @Override
    public int getBottomInset() {
        return isDrawingToEdge() ? (int) Math.ceil(mSystemInsets.bottom * mPxToDp) : 0;
    }

    @Override
    public int getBottomInsetPx() {
        return isDrawingToEdge() ? mSystemInsets.bottom : 0;
    }

    @Override
    public int getSystemBottomInsetPx() {
        return mSystemInsets.bottom;
    }

    @Override
    public boolean isDrawingToEdge() {
        return mIsDrawingToEdge;
    }

    @Override
    public boolean isPageOptedIntoEdgeToEdge() {
        return mIsPageOptedIntoEdgeToEdge;
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

    // LayoutStateProvider.LayoutStateObserver

    @Override
    public void onStartedShowing(int layoutType) {
        drawToEdge(mIsPageOptedIntoEdgeToEdge, false);
    }

    // FullscreenManager.Observer
    @Override
    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
        drawToEdge(mIsPageOptedIntoEdgeToEdge, /* changedWindowState= */ true);
    }

    @Override
    public void onExitFullscreen(Tab tab) {
        drawToEdge(mIsPageOptedIntoEdgeToEdge, /* changedWindowState= */ true);
    }

    private View getContentView() {
        return mActivity.findViewById(ROOT_UI_VIEW_ID);
    }

    private void updateBrowserControlsVisibility(boolean visible) {
        if (mBottomControlsAreVisible == visible) {
            return;
        }
        mBottomControlsAreVisible = visible;
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
                        drawToEdge(
                                EdgeToEdgeUtils.isPageOptedIntoEdgeToEdge(mCurrentTab, value),
                                /* changedWindowState= */ false);
                    }

                    @Override
                    public void safeAreaConstraintChanged(boolean hasConstraint) {
                        if (mHasSafeAreaConstraint == hasConstraint) {
                            return;
                        }

                        mHasSafeAreaConstraint = hasConstraint;
                        for (var observer : mEdgeChangeObservers) {
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
        drawToEdge(
                EdgeToEdgeUtils.isPageOptedIntoEdgeToEdge(mCurrentTab),
                /* changedWindowState= */ false);
    }

    /**
     * Conditionally draws the given View ToEdge or ToNormal based on the {@code toEdge} param.
     *
     * @param pageOptedIntoEdgeToEdge Whether the page is opted into edge-to-edge.
     * @param changedWindowState Whether this method is called due to window state changed (e.g.
     *     windowInsets updated, window goes into fullscreen mode).
     */
    @VisibleForTesting
    void drawToEdge(boolean pageOptedIntoEdgeToEdge, boolean changedWindowState) {
        final boolean isChinEnabled = isSupportedByConfiguration(mActivity, mInsetObserver);

        if (!isChinEnabled) {
            RecordHistogram.recordBooleanHistogram(
                    DRAW_TO_EDGE_UNSUPPORTED_CONFIG_HISTOGRAM, changedWindowState);
        }

        // Exit early if there is a tappable navbar (3-button) as the controller should not function
        // when 3-button nav is enabled.
        if (!shouldMonitorConfigurationChanges()
                && EdgeToEdgeUtils.hasTappableNavigationBar(mActivity.getWindow())) {
            return;
        }

        @LayoutType
        int currentLayoutType =
                mLayoutManager != null ? mLayoutManager.getActiveLayoutType() : LayoutType.NONE;
        boolean shouldDrawToEdge =
                EdgeToEdgeUtils.shouldDrawToEdge(
                        pageOptedIntoEdgeToEdge, currentLayoutType, mSystemInsets.bottom);
        if (shouldMonitorConfigurationChanges()) {
            shouldDrawToEdge &= isChinEnabled;
            pageOptedIntoEdgeToEdge &= isChinEnabled;
        }
        // Refresh the mHasSafeAreaConstraint to ensure the boolean stays fresh (e.g. when
        // #drawToEdge is called due to tab switching)
        boolean hasSafeAreaConstraint = EdgeToEdgeUtils.hasSafeAreaConstraintForTab(mCurrentTab);

        boolean changedPageOptedIn = pageOptedIntoEdgeToEdge != mIsPageOptedIntoEdgeToEdge;
        boolean changedDrawToEdge = shouldDrawToEdge != mIsDrawingToEdge;
        boolean changedSafeAreaConstraint = mHasSafeAreaConstraint != hasSafeAreaConstraint;
        mIsPageOptedIntoEdgeToEdge = pageOptedIntoEdgeToEdge;
        mIsDrawingToEdge = shouldDrawToEdge;
        mHasSafeAreaConstraint = hasSafeAreaConstraint;

        if (changedPageOptedIn) {
            Log.v(
                    TAG,
                    "Switching %s",
                    (mIsPageOptedIntoEdgeToEdge
                            ? "Opted into EdgeToEdge"
                            : "Not opted into EdgeToEdge"));
        }

        if (changedDrawToEdge) {
            Log.v(TAG, "Switching %s", (mIsDrawingToEdge ? "ToEdge" : "ToNormal"));
        }

        if (changedPageOptedIn || changedDrawToEdge || changedWindowState) {
            adjustEdgePaddings();
            pushSafeAreaInsetUpdate();
            updatePadAdjusters();

            for (var observer : mEdgeChangeObservers) {
                observer.onToEdgeChange(
                        mSystemInsets.bottom, isDrawingToEdge(), isPageOptedIntoEdgeToEdge());
            }
        }

        if (changedSafeAreaConstraint) {
            for (var observer : mEdgeChangeObservers) {
                observer.onSafeAreaConstraintChanged(mHasSafeAreaConstraint);
            }
        }
    }

    private void verifyInsetsInSupportedConfiguration(WindowInsetsCompat windowInsets) {
        // Check for the presence of a tappable element (in case the navigation bar inset is
        // missing for some reason) for logging purposes.
        Insets tappableElementInsets =
                windowInsets.getInsets(WindowInsetsCompat.Type.tappableElement());
        // The navigation bar will never be at the top.
        boolean tappableElement =
                tappableElementInsets.bottom > 0
                        || tappableElementInsets.left > 0
                        || tappableElementInsets.right > 0;

        // Check whether the device appears to be in gesture navigation mode.
        boolean isGestureNavigation = EdgeToEdgeUtils.isInGestureNavigationMode(windowInsets);
        @SupportedConfigurationStrangeInsetsState int state;
        if (tappableElement) {
            if (isGestureNavigation) {
                state = SupportedConfigurationStrangeInsetsState.ERROR_TAPPABLE_ELEMENT_GESTURE_NAV;
            } else {
                state = SupportedConfigurationStrangeInsetsState.TAPPABLE_ELEMENT_NOT_GESTURE_NAV;
            }
        } else {
            if (isGestureNavigation) {
                // !tappableElement && isGestureNavigation is intended
                return;
            } else {
                state =
                        SupportedConfigurationStrangeInsetsState
                                .NO_TAPPABLE_ELEMENT_NOT_GESTURE_NAV;
            }
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.EdgeToEdge.Debugging.SupportedConfigurationStrangeInsets",
                state,
                SupportedConfigurationStrangeInsetsState.NUM_ENTRIES);
    }

    @VisibleForTesting
    WindowInsetsCompat handleWindowInsets(View rootView, WindowInsetsCompat windowInsets) {
        boolean changedWindowState = false;
        @SupportedConfigurationSwitch
        int configurationChanged = SupportedConfigurationSwitch.NUM_ENTRIES;
        if (mIsBottomChinEnabled != isSupportedByConfiguration(mActivity, mInsetObserver)) {
            Log.v(
                    TAG,
                    "Switching supported configuration from %s",
                    (mIsBottomChinEnabled
                            ? "supported to unsupported"
                            : "unsupported to supported"));
            configurationChanged =
                    mIsBottomChinEnabled
                            ? SupportedConfigurationSwitch.FROM_SUPPORTED_TO_UNSUPPORTED
                            : SupportedConfigurationSwitch.FROM_UNSUPPORTED_TO_SUPPORTED;
            RecordHistogram.recordEnumeratedHistogram(
                    SUPPORTED_CONFIGURATION_SWITCH_HISTOGRAM,
                    configurationChanged,
                    SupportedConfigurationSwitch.NUM_ENTRIES);
            mIsBottomChinEnabled = isSupportedByConfiguration(mActivity, mInsetObserver);
            changedWindowState = true;
        }
        if (mIsBottomChinEnabled) {
            verifyInsetsInSupportedConfiguration(windowInsets);
        }

        // Exit early if there is a tappable navbar (3-button) as the controller should not function
        // when 3-button nav is enabled.
        if (!shouldMonitorConfigurationChanges()
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
            mIsPageOptedIntoEdgeToEdge =
                    mIsPageOptedIntoEdgeToEdge
                            && isSupportedByConfiguration(mActivity, mInsetObserver);

            changedWindowState = true;
        }

        // Note that we cannot call #drawToEdge earlier since we need the system
        // insets.
        if (changedWindowState) {
            drawToEdge(mIsPageOptedIntoEdgeToEdge, /* changedWindowState= */ true);
        }

        // Signal: When configuration is changed, did we pad the system correctly.
        if (configurationChanged == SupportedConfigurationSwitch.FROM_SUPPORTED_TO_UNSUPPORTED) {
            recordConfigurationSwitchScenario(
                    originalSystemInsets, newInsets, mAppliedContentViewPadding);
        }

        var builder = new WindowInsetsCompat.Builder(windowInsets);

        // Consume top insets only when in fullscreen, where we are forcing 0 as the top padding.
        if (mAppliedContentViewPadding.top == 0) {
            builder.setInsets(WindowInsetsCompat.Type.statusBars(), Insets.NONE);
            builder.setInsets(WindowInsetsCompat.Type.captionBar(), Insets.NONE);
        }
        Insets mandatorySystemGestures =
                windowInsets.getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        if (mAppliedContentViewPadding.bottom == 0) {
            builder.setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE);
            builder.setInsets(WindowInsetsCompat.Type.tappableElement(), Insets.NONE);
            builder.setInsets(WindowInsetsCompat.Type.ime(), Insets.NONE);
            mandatorySystemGestures =
                    Insets.of(
                            mandatorySystemGestures.left,
                            mandatorySystemGestures.top,
                            mandatorySystemGestures.right,
                            0);
        }
        builder.setInsets(
                WindowInsetsCompat.Type.mandatorySystemGestures(), mandatorySystemGestures);
        return builder.build();
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
     * The {@link EdgeToEdgePadAdjuster}s should only be padded with an extra bottom inset if the
     * activity is currently in edge-to-edge, and if the adjusters aren't already positioned above
     * the system insets due to the keyboard or the bottom controls being visible.
     */
    private boolean shouldPadAdjusters() {
        // Never pad the adjusters if the keyboard is visible.
        if (mKeyboardInsets != null && mKeyboardInsets.bottom > 0) return false;

        // Never pad the adjusters if the bottom controls are visible. Except in the tab switcher.
        @LayoutType
        int currentLayoutType =
                mLayoutManager != null ? mLayoutManager.getActiveLayoutType() : LayoutType.NONE;
        if (mBottomControlsAreVisible && currentLayoutType != LayoutType.TAB_SWITCHER) return false;

        // Pad the adjusters if drawing to edge.
        return isDrawingToEdge();
    }

    private void updatePadAdjusters() {
        boolean shouldPad = shouldPadAdjusters();
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
            mEdgeToEdgeManager.setContentFitsWindowInsets(!isDrawingToEdge());
        }

        View contentView = getContentView();
        assert contentView != null : "Root view for Edge To Edge not found!";

        int topPadding = mSystemInsets.top;
        // Adjust the bottom padding to reflect whether ToEdge or ToNormal for the Gesture Nav Bar.
        // All the other edges need to be padded to prevent drawing under an edge that we
        // don't want drawn ToEdge (e.g. the Status Bar).
        int bottomPadding = mIsDrawingToEdge ? 0 : mSystemInsets.bottom;
        if (mKeyboardInsets != null && mKeyboardInsets.bottom > bottomPadding) {
            // If the keyboard is showing, change the bottom padding to account for the keyboard.
            // Clear the bottom inset used for the adjusters, since there are no missing bottom
            // system bars above the keyboard to compensate for.
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
                (mIsDrawingToEdge && !mFullscreenManager.getPersistentFullscreenMode());

        // When the content view is padded (e.g. when keyboard is showing), we should not count the
        // padding as part of the bottom safe area.
        int safeAreaInsets = Math.max(mSystemInsets.bottom - mAppliedContentViewPadding.bottom, 0);

        int bottomInsetOnSafeArea = hasBottomSafeArea ? safeAreaInsets : 0;
        mInsetObserver.updateBottomInsetForEdgeToEdge(bottomInsetOnSafeArea);
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
        mEdgeToEdgeStateProvider.releaseSetDecorFitsSystemWindowToken(mEdgeToEdgeToken);
    }

    static void recordConfigurationSwitchScenario(
            Insets originalInsets, Insets newInsets, Insets paddingApplied) {
        // Do not record when configuration change is disabled.
        if (!shouldMonitorConfigurationChanges()) return;

        // Do not record landscape mode. Assuming the configuration change will be triggered
        // mostly with nav bar in portrait mode.
        if (paddingApplied.left > 0 || paddingApplied.right > 0) return;

        @ConfigurationSwitchOutcome int outcome;
        // Correct cases - fixed applied
        if (paddingApplied.bottom > 0) {
            if (originalInsets.bottom != 0) {
                outcome = ConfigurationSwitchOutcome.ADD_PADDING_ORIGINAL_INSETS;
            } else if (newInsets.bottom != 0) {
                outcome = ConfigurationSwitchOutcome.ADD_PADDING_NEW_INSETS;
            } else {
                outcome = ConfigurationSwitchOutcome.ERROR_ADD_PADDING_BOTH_INSETS_EMPTY;
            }
        } else { // paddingApplied.bottom == 0
            if (originalInsets.bottom == 0 && newInsets.bottom == 0) {
                outcome = ConfigurationSwitchOutcome.NO_PADDING_BOTH_INSETS_EMPTY;
            } else if (originalInsets.bottom > 0) {
                outcome = ConfigurationSwitchOutcome.NO_PADDING_NO_NEW_INSETS;
            } else {
                outcome = ConfigurationSwitchOutcome.ERROR_NO_PADDING_WITH_NEW_INSETS;
            }
        }

        RecordHistogram.recordEnumeratedHistogram(
                CONFIGURATION_SWITCH_OUTCOME_HISTOGRAM,
                outcome,
                ConfigurationSwitchOutcome.NUM_ENTRIES);
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

    public void setIsOptedIntoEdgeToEdgeForTesting(boolean toEdge) {
        mIsPageOptedIntoEdgeToEdge = toEdge;
    }

    public void setIsDrawingToEdgeForTesting(boolean toEdge) {
        mIsDrawingToEdge = toEdge;
    }

    public @Nullable ChangeObserver getAnyChangeObserverForTesting() {
        return mEdgeChangeObservers.isEmpty() ? null : mEdgeChangeObservers.iterator().next();
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
