// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent.BOTTOM_SHEET_EXPANDED;
import static org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent.BOTTOM_SHEET_PEEKING;
import static org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent.DISMISSED_FROM_PEEKING_STATE;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.text.format.DateUtils;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.page_insights.proto.Config.PageInsightsConfig;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.Page;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceRenderer;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceScope;
import org.chromium.chrome.browser.xsurface_provider.XSurfaceProcessScopeProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.function.BooleanSupplier;
import java.util.function.Function;

/**
 * PageInsights mediator component listening to various external events to update UI, internal
 * states accordingly:
 * <ul>
 * <li> Observes browser controls for hide-on-scroll behavior
 * <li> Closes the sheet when the Tab page gets reloaded
 * <li> Resizes contents upon Sheet offset/state changes
 * <li> Adjusts the top corner radius to the sheet height
 * </ul>
 */
public class PageInsightsMediator extends EmptyTabObserver implements BottomSheetObserver {
    private static final String TAG = "PIMediator";

    static final int DEFAULT_TRIGGER_DELAY_MS = (int) DateUtils.MINUTE_IN_MILLIS;
    private static final float MINIMUM_CONFIDENCE = 0.5f;
    static final String PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END =
            "page_insights_can_autotrigger_after_end";
    static final String PAGE_INSIGHTS_CAN_AUTOTRIGGER_WHILE_IN_MOTION =
            "page_insights_can_autotrigger_while_in_motion";
    static final String PAGE_INSIGHTS_CAN_RETURN_TO_PEEK_AFTER_EXPANSION =
            "page_insights_can_return_to_peek_after_expansion";

    private final PageInsightsSheetContent mSheetContent;
    private final ManagedBottomSheetController mSheetController;
    private final Context mContext;

    // BottomSheetController for other bottom sheet UIs.
    private final BottomSheetController mBottomUiController;

    // Observers other bottom sheet UI state.
    private final BottomSheetObserver mBottomUiObserver;

    // Bottom browser controls resizer. Used to resize web contents to move up bottom-aligned
    // elements such as cookie dialog.
    private final BrowserControlsSizer mBrowserControlsSizer;

    private final BrowserControlsStateProvider mControlsStateProvider;

    // Browser controls observer. Monitors the browser controls offset changes to scroll
    // away the bottom sheet in sync with the controls.
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;

    private final ExpandedSheetHelper mExpandedSheetHelper;

    // Sheet background drawable whose top corners needs to be rounded.
    private GradientDrawable mBackgroundDrawable;

    private int mMaxCornerRadiusPx;
    private View mSheetContainer;

    private final BooleanSupplier mIsPageInsightsEnabledSupplier;
    private final Function<NavigationHandle, PageInsightsConfig> mPageInsightsConfigProvider;
    private final Handler mHandler;
    private final Runnable mAutoTriggerTimerRunnable = this::onAutoTriggerTimerFinished;
    private final Callback<Boolean> mInMotionCallback = inMotion -> maybeAutoTrigger();
    private final PageInsightsSheetContent.OnBottomSheetTouchHandler mOnBottomSheetTouchHandler =
            new PageInsightsSheetContent.OnBottomSheetTouchHandler() {
                @Override
                public boolean handleTap() {
                    return handleBottomSheetTap();
                }

                @Override
                public boolean shouldInterceptTouchEvents() {
                    return shouldInterceptBottomSheetTouchEvents();
                }
            };
    private final HashMap<String, Object> mSurfaceRendererContextValues;
    private final ObservableSupplier<Tab> mTabObservable;
    private final Supplier<Profile> mProfileSupplier;
    private final ObservableSupplierImpl<Boolean> mWillHandleBackPressSupplier;
    private final boolean mIsAccessibilityEnabled;
    private final boolean mCanAutoTriggerWhileInMotion;
    private final boolean mCanReturnToPeekAfterExpansion;
    @Nullable private final ObservableSupplier<Boolean> mInMotionSupplier;
    @Nullable private final BackPressManager mBackPressManager;
    @Nullable private final BackPressHandler mBackPressHandler;

    private PageInsightsDataLoader mPageInsightsDataLoader;
    @Nullable private PageInsightsSurfaceRenderer mSurfaceRenderer;
    @Nullable private PageInsightsMetadata mCurrentMetadata;
    @Nullable private PageInsightsConfig mCurrentConfig;
    @Nullable private View mCurrentFeedView;
    @Nullable private View mCurrentChildView;
    private boolean mIsShowingChildView;
    @Nullable private NavigationHandle mCurrentNavigationHandle;

    // Caches the sheet height at the current state. Avoids the repeated call to resize the content
    // if the size hasn't changed since.
    private int mCachedSheetHeight;

    // Whether the sheet was hidden due to another bottom sheet UI, and needs to be restored
    // when notified when the UI was closed.
    private boolean mShouldRestore;

    // Amount of time to wait before triggering the sheet automatically. Can be overridden
    // for testing.
    private long mAutoTriggerDelayMs;

    private int mOldState = SheetState.NONE;

    @IntDef({
        AutoTriggerStage.CANCELLED_OR_NOT_STARTED,
        AutoTriggerStage.AWAITING_TIMER,
        AutoTriggerStage.AWAITING_NAV_HANDLE,
        AutoTriggerStage.FETCHING_DATA,
        AutoTriggerStage.READY_FOR_AUTO_TRIGGER,
        AutoTriggerStage.AUTO_TRIGGERED
    })
    @interface AutoTriggerStage {
        int CANCELLED_OR_NOT_STARTED = 0;
        int AWAITING_TIMER = 1;
        // This stage will be skipped if nav handle already available when timer finishes.
        int AWAITING_NAV_HANDLE = 2;
        int FETCHING_DATA = 3;
        int READY_FOR_AUTO_TRIGGER = 4;
        int AUTO_TRIGGERED = 5;
    }

    private @AutoTriggerStage int mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        PageInsightsEvent.USER_INVOKES_PIH,
        PageInsightsEvent.AUTO_PEEK_TRIGGERED,
        PageInsightsEvent.STATE_PEEK,
        PageInsightsEvent.STATE_EXPANDED,
        PageInsightsEvent.DISMISS_PEEK,
        PageInsightsEvent.DISMISS_EXPANDED,
        PageInsightsEvent.TAP_XSURFACE_VIEW_URL,
        PageInsightsEvent.TAP_XSURFACE_VIEW_SHARE,
        PageInsightsEvent.TAP_XSURFACE_VIEW_CHILD_PAGE,
        PageInsightsEvent.COUNT
    })
    @interface PageInsightsEvent {
        int USER_INVOKES_PIH = 0;
        int AUTO_PEEK_TRIGGERED = 1;
        int STATE_PEEK = 2;
        int STATE_EXPANDED = 3;
        int DISMISS_PEEK = 4;
        int DISMISS_EXPANDED = 5;
        // User interacts with a xSurface card with URL in PIH
        int TAP_XSURFACE_VIEW_URL = 6;
        // User interacts with a xSurface share functionality in PIH
        int TAP_XSURFACE_VIEW_SHARE = 7;
        // User interacts with a xSurface child page in PIH
        int TAP_XSURFACE_VIEW_CHILD_PAGE = 8;
        int COUNT = 9;
    }

    private static void logPageInsightsEvent(@PageInsightsEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.PageInsights.Event", event, PageInsightsEvent.COUNT);
    }

    public PageInsightsMediator(
            Context context,
            View layoutView,
            ObservableSupplier<Tab> tabObservable,
            Supplier<ShareDelegate> shareDelegateSupplier,
            Supplier<Profile> profileSupplier,
            ManagedBottomSheetController bottomSheetController,
            BottomSheetController bottomUiController,
            ExpandedSheetHelper expandedSheetHelper,
            BrowserControlsStateProvider controlsStateProvider,
            BrowserControlsSizer browserControlsSizer,
            @Nullable BackPressManager backPressManager,
            @Nullable ObservableSupplier<Boolean> inMotionSupplier,
            BooleanSupplier isPageInsightsEnabledSupplier,
            Function<NavigationHandle, PageInsightsConfig> pageInsightsConfigProvider) {
        mContext = context;
        mTabObservable = tabObservable;
        mProfileSupplier = profileSupplier;
        mControlsStateProvider = controlsStateProvider;
        mInMotionSupplier = inMotionSupplier;
        mWillHandleBackPressSupplier = new ObservableSupplierImpl<>(false);
        mSheetContent =
                new PageInsightsSheetContent(
                        mContext,
                        layoutView,
                        view -> loadMyActivityUrl(tabObservable),
                        this::handleBackPress,
                        mWillHandleBackPressSupplier,
                        mOnBottomSheetTouchHandler);
        mSheetController = bottomSheetController;
        mBottomUiController = bottomUiController;
        mExpandedSheetHelper = expandedSheetHelper;
        mHandler = new Handler(Looper.getMainLooper());
        mBrowserControlsSizer = browserControlsSizer;
        mBrowserControlsObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate) {
                        bottomSheetController.setBrowserControlsHiddenRatio(
                                controlsStateProvider.getBrowserControlHiddenRatio());
                        maybeAutoTrigger();
                    }
                };
        controlsStateProvider.addObserver(mBrowserControlsObserver);
        bottomSheetController.addObserver(this);
        if (mInMotionSupplier != null) {
            mInMotionSupplier.addObserver(mInMotionCallback);
        }
        mBottomUiObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(@SheetState int newState, int reason) {
                        onBottomUiStateChanged(newState >= SheetState.PEEK);
                    }
                };
        bottomUiController.addObserver(mBottomUiObserver);
        mIsPageInsightsEnabledSupplier = isPageInsightsEnabledSupplier;
        mPageInsightsConfigProvider = pageInsightsConfigProvider;
        mPageInsightsDataLoader = new PageInsightsDataLoader();
        mIsAccessibilityEnabled = ChromeAccessibilityUtil.get().isAccessibilityEnabled();
        mSurfaceRendererContextValues =
                PageInsightsActionHandlerImpl.createContextValues(
                        new PageInsightsActionHandlerImpl(
                                tabObservable,
                                shareDelegateSupplier,
                                this::changeToChildPage,
                                PageInsightsMediator::logPageInsightsEvent));
        mAutoTriggerDelayMs =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                        PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END,
                        DEFAULT_TRIGGER_DELAY_MS);
        mCanAutoTriggerWhileInMotion =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                        PAGE_INSIGHTS_CAN_AUTOTRIGGER_WHILE_IN_MOTION,
                        false);
        mCanReturnToPeekAfterExpansion =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                        PAGE_INSIGHTS_CAN_RETURN_TO_PEEK_AFTER_EXPANSION,
                        false);
        if (tabObservable.get() != null) {
            onTab(tabObservable.get());
        } else {
            tabObservable.addObserver(
                    tab -> {
                        if (tab == null) return;
                        onTab(tab);
                    });
        }
        mBackPressManager = backPressManager;
        if (BackPressManager.isEnabled()) {
            mBackPressHandler = bottomSheetController.getBottomSheetBackPressHandler();
            if (mBackPressHandler != null
                    && backPressManager != null
                    && !backPressManager.has(BackPressHandler.Type.PAGE_INSIGHTS_BOTTOM_SHEET)) {
                backPressManager.addHandler(
                        mBackPressHandler, BackPressHandler.Type.PAGE_INSIGHTS_BOTTOM_SHEET);
            }
        } else {
            mBackPressHandler = null;
        }
    }

    void initView(View bottomSheetContainer) {
        mSheetContainer = bottomSheetContainer;
        View view = bottomSheetContainer.findViewById(R.id.background);
        mBackgroundDrawable = (GradientDrawable) view.getBackground();
        mMaxCornerRadiusPx =
                bottomSheetContainer
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_corner_radius);
        setCornerRadiusPx(0);

        // Initialize the hidden ratio, otherwise it won't be set until the first offset
        // change event occurs.
        mSheetController.setBrowserControlsHiddenRatio(
                mControlsStateProvider.getBrowserControlHiddenRatio());
    }

    void onBottomUiStateChanged(boolean opened) {
        if (opened && shouldHideContent()) {
            mSheetController.hideContent(mSheetContent, true);
            mShouldRestore = true;
        } else if (!opened && mShouldRestore) {
            mSheetController.requestShowContent(mSheetContent, true);
            mShouldRestore = false;
        }
    }

    private void onTab(Tab tab) {
        delayStartAutoTrigger(mAutoTriggerDelayMs);
        tab.addObserver(this);
    }

    private boolean shouldHideContent() {
        // See if we need to hide the sheet content temporarily while another bottom UI is
        // launched. No need to hide if not in peek/full state or in scrolled-away state,
        // hence not visible.
        return mSheetController.getSheetState() >= SheetState.PEEK && !isInScrolledAwayState();
    }

    private boolean isInScrolledAwayState() {
        return !MathUtils.areFloatsEqual(mControlsStateProvider.getBrowserControlHiddenRatio(), 0f);
    }

    private boolean handleBottomSheetTap() {
        if (mSheetController.getSheetState() == BottomSheetController.SheetState.PEEK) {
            mSheetController.expandSheet();
            return true;
        }
        return false;
    }

    private boolean shouldInterceptBottomSheetTouchEvents() {
        return mSheetController.getSheetState() == BottomSheetController.SheetState.PEEK;
    }

    private boolean handleBackPress() {
        if (mSheetController.getSheetState() != BottomSheetController.SheetState.FULL) {
            return false;
        }

        if (mIsShowingChildView) {
            mSheetContent.showFeedPage();
            mIsShowingChildView = false;
        } else if (!mSheetController.collapseSheet(true)) {
            mSheetController.hideContent(mSheetContent, true);
        }
        return true;
    }

    private void cancelAutoTrigger() {
        mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
        mHandler.removeCallbacks(mAutoTriggerTimerRunnable);
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        Log.v(TAG, "onPageLoadStarted");
        mCurrentNavigationHandle = null;
        mCurrentMetadata = null;
        mCurrentConfig = null;
        cancelAutoTrigger();
        if (mSheetContent == mSheetController.getCurrentSheetContent()) {
            mSheetController.hideContent(mSheetContent, true);
        }
        delayStartAutoTrigger(mAutoTriggerDelayMs);
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(
            Tab tab, NavigationHandle navigationHandle) {
        Log.v(TAG, "onDidFinishNavigationInPrimaryMainFrame");
        mCurrentNavigationHandle = navigationHandle;
        if (mAutoTriggerStage == AutoTriggerStage.AWAITING_NAV_HANDLE) {
            maybeFetchDataForAutoTrigger();
        }
    }

    private void delayStartAutoTrigger(long delayMs) {
        mAutoTriggerStage = AutoTriggerStage.AWAITING_TIMER;
        if (delayMs > 0) {
            mHandler.postDelayed(mAutoTriggerTimerRunnable, delayMs);
        } else {
            mAutoTriggerTimerRunnable.run();
        }
    }

    @VisibleForTesting
    void onAutoTriggerTimerFinished() {
        if (mAutoTriggerStage == AutoTriggerStage.AWAITING_TIMER) {
            maybeFetchDataForAutoTrigger();
        }
    }

    private void maybeFetchDataForAutoTrigger() {
        if (mCurrentNavigationHandle == null) {
            mAutoTriggerStage = AutoTriggerStage.AWAITING_NAV_HANDLE;
            return;
        }
        PageInsightsConfig config = mPageInsightsConfigProvider.apply(mCurrentNavigationHandle);

        if (!shouldFetchDataForAutoTrigger(config)) {
            mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
            return;
        }
        if (mTabObservable.get() == null) {
            Log.e(TAG, "Cancelling auto-trigger because Tab is unexpectedly null.");
            mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
            return;
        }

        mAutoTriggerStage = AutoTriggerStage.FETCHING_DATA;
        Log.v(TAG, "Fetching data for auto-trigger");
        mPageInsightsDataLoader.loadInsightsData(
                mTabObservable.get().getUrl(),
                config.getShouldAttachGaiaToRequest(),
                metadata -> {
                    if (mAutoTriggerStage != AutoTriggerStage.FETCHING_DATA) {
                        // Don't proceed if something has changed since we started fetching data.
                        return;
                    }
                    if (metadata.getAutoPeekConditions().getConfidence() > MINIMUM_CONFIDENCE) {
                        mCurrentMetadata = metadata;
                        mCurrentConfig = config;
                        mAutoTriggerStage = AutoTriggerStage.READY_FOR_AUTO_TRIGGER;
                        maybeAutoTrigger();
                    } else {
                        mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
                        Log.v(TAG, "Cancelling auto-trigger as confidence too low");
                    }
                });
    }

    private void maybeAutoTrigger() {
        if (mAutoTriggerStage != AutoTriggerStage.READY_FOR_AUTO_TRIGGER) return;

        if (!BrowserControlsUtils.areBrowserControlsOffScreen(mControlsStateProvider)
                && !mIsAccessibilityEnabled) {
            Log.v(
                    TAG,
                    "Not auto-triggering because browser controls are not off screen and a11y is"
                            + " not enabled.");
            return;
        }
        if (!mCanAutoTriggerWhileInMotion
                && mInMotionSupplier != null
                && mInMotionSupplier.get() != null
                && mInMotionSupplier.get()) {
            Log.v(TAG, "Not auto-triggering because compositor is in motion.");
            return;
        }

        if (mSheetContent == mSheetController.getCurrentSheetContent()) {
            Log.v(
                    TAG,
                    "Cancelling auto-trigger because page insights sheet content already being"
                            + " shown.");
            mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
            return;
        }
        if (mCurrentMetadata == null || mCurrentConfig == null) {
            Log.e(TAG, "Cancelling  auto-trigger because metadata or config unexpectedly null.");
            mAutoTriggerStage = AutoTriggerStage.CANCELLED_OR_NOT_STARTED;
            return;
        }

        Log.v(TAG, "Auto-triggering.");
        if (mCurrentConfig.getShouldXsurfaceLog()) {
            getSurfaceRenderer()
                    .onSurfaceCreated(
                            PageInsightsLoggingParametersImpl.create(
                                    mProfileSupplier.get(), mCurrentMetadata));
        }
        initSheetContent(
                mCurrentMetadata,
                /* isPrivacyNoticeRequired= */ mCurrentConfig.getShouldXsurfaceLog(),
                /* shouldHavePeekState= */ true);
        logPageInsightsEvent(PageInsightsEvent.AUTO_PEEK_TRIGGERED);
        getSurfaceRenderer().onEvent(BOTTOM_SHEET_PEEKING);
        mSheetController.requestShowContent(mSheetContent, true);
        mAutoTriggerStage = AutoTriggerStage.AUTO_TRIGGERED;
    }

    private boolean shouldFetchDataForAutoTrigger(PageInsightsConfig config) {
        if (mSheetContent == mSheetController.getCurrentSheetContent()) {
            Log.v(
                    TAG,
                    "Not fetching data for auto-trigger because page insights sheet content"
                            + " already being shown.");
            return false;
        }
        if (!mIsPageInsightsEnabledSupplier.getAsBoolean()) {
            Log.v(TAG, "Not fetching data for auto-trigger because feature is disabled.");
            return false;
        }
        if (!config.getShouldAutoTrigger()) {
            Log.v(TAG, "Not fetching data for auto-trigger because auto-triggering is disabled.");
            return false;
        }
        return true;
    }

    void launch() {
        cancelAutoTrigger();
        mSheetContent.showLoadingIndicator();
        mSheetController.requestShowContent(mSheetContent, true);
        if (mTabObservable.get() == null) {
            Log.e(TAG, "Can't launch Page Insights because Tab is null.");
            return;
        }
        mCurrentConfig = mPageInsightsConfigProvider.apply(mCurrentNavigationHandle);
        mPageInsightsDataLoader.loadInsightsData(
                mTabObservable.get().getUrl(),
                mCurrentConfig.getShouldAttachGaiaToRequest(),
                metadata -> {
                    mCurrentMetadata = metadata;
                    if (mCurrentConfig.getShouldXsurfaceLog()) {
                        getSurfaceRenderer()
                                .onSurfaceCreated(
                                        PageInsightsLoggingParametersImpl.create(
                                                mProfileSupplier.get(), metadata));
                    }
                    initSheetContent(
                            metadata,
                            /* isPrivacyNoticeRequired= */ mCurrentConfig.getShouldXsurfaceLog(),
                            /* shouldHavePeekState= */ false);
                    setBackgroundColors(/* ratioOfCompletionFromPeekToExpanded= */ 1.0f);
                    setCornerRadiusPx(mMaxCornerRadiusPx);
                    logPageInsightsEvent(PageInsightsEvent.USER_INVOKES_PIH);
                    // We need to perform this logging here, even though we also do it when the
                    // sheet reaches expanded state, as XSurface logging is not initialised until
                    // now.
                    getSurfaceRenderer().onEvent(BOTTOM_SHEET_EXPANDED);
                    mSheetController.expandSheet();
                });
    }

    private void initSheetContent(
            PageInsightsMetadata metadata,
            boolean isPrivacyNoticeRequired,
            boolean shouldHavePeekState) {
        mCurrentFeedView = getXSurfaceView(metadata.getFeedPage().getElementsOutput());
        mSheetContent.initContent(mCurrentFeedView, isPrivacyNoticeRequired, shouldHavePeekState);
        mSheetContent.showFeedPage();
    }

    private View getXSurfaceView(ByteString elementsOutput) {
        return getSurfaceRenderer()
                .render(elementsOutput.toByteArray(), mSurfaceRendererContextValues);
    }

    private void changeToChildPage(int id) {
        if (mCurrentMetadata == null) {
            return;
        }

        for (int i = 0; i < mCurrentMetadata.getPagesCount(); i++) {
            Page currPage = mCurrentMetadata.getPages(i);
            if (id == currPage.getId().getNumber()) {
                if (mCurrentChildView != null) {
                    getSurfaceRenderer().unbindView(mCurrentChildView);
                }
                mCurrentChildView = getXSurfaceView(currPage.getElementsOutput());
                mSheetContent.showChildPage(mCurrentChildView, currPage.getTitle());
                mIsShowingChildView = true;
            }
        }
    }

    private void loadMyActivityUrl(Supplier<Tab> currTabObserver) {
        Tab currTab = currTabObserver.get();
        if (currTab != null) {
            currTab.loadUrl(new LoadUrlParams(UrlConstants.MY_ACTIVITY_HOME_URL));
        }
    }

    PageInsightsSheetContent getSheetContent() {
        return mSheetContent;
    }

    // BottomSheetObserver

    @Override
    public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
        if (newState == SheetState.HIDDEN) {
            mWillHandleBackPressSupplier.set(false);
            setBottomControlsHeight(mSheetController.getCurrentOffset());
            handleDismissal(mOldState);
        } else if (newState == SheetState.PEEK) {
            mWillHandleBackPressSupplier.set(false);
            setBottomControlsHeight(mSheetController.getCurrentOffset());
            setBackgroundColors(/* ratioOfCompletionFromPeekToExpanded= */ .0f);
            // The user should always be able to swipe to dismiss from peek state.
            mSheetContent.setSwipeToDismissEnabled(true);
            logPageInsightsEvent(PageInsightsEvent.STATE_PEEK);
            // We don't log peek state to XSurface here, as its BOTTOM_SHEET_PEEKING event is only
            // intended for when the feature initially auto-peeks.
        } else if (newState == SheetState.FULL) {
            mWillHandleBackPressSupplier.set(true);
            setBackgroundColors(/* ratioOfCompletionFromPeekToExpanded= */ 1.0f);
            if (mOldState == SheetState.PEEK && mCanReturnToPeekAfterExpansion) {
                // Disable swiping to dismiss, so that swiping/scrim-tapping returns to peek state
                // instead.
                mSheetContent.setSwipeToDismissEnabled(false);
            } else if (mOldState != SheetState.FULL) {
                // Enable swiping to dismiss, and also explicitly disable peek state. If peek state
                // remains enabled then some lighter swipes can return to it, even with
                // swipeToDismissEnabled true.
                mSheetContent.setSwipeToDismissEnabled(true);
                mSheetContent.setShouldHavePeekState(false);
            }
            logPageInsightsEvent(PageInsightsEvent.STATE_EXPANDED);
            getSurfaceRenderer().onEvent(BOTTOM_SHEET_EXPANDED);
        } else {
            mWillHandleBackPressSupplier.set(false);
        }

        if (newState != SheetState.NONE && newState != SheetState.SCROLLING) {
            mOldState = newState;
        }
    }

    private void handleDismissal(@SheetState int oldState) {
        mIsShowingChildView = false;

        if (mCurrentFeedView != null) {
            getSurfaceRenderer().unbindView(mCurrentFeedView);
        }
        if (mCurrentChildView != null) {
            getSurfaceRenderer().unbindView(mCurrentChildView);
        }

        if (mOldState == SheetState.PEEK) {
            logPageInsightsEvent(PageInsightsEvent.DISMISS_PEEK);
            getSurfaceRenderer().onEvent(DISMISSED_FROM_PEEKING_STATE);
        } else if (mOldState >= SheetState.HALF) {
            logPageInsightsEvent(PageInsightsEvent.DISMISS_EXPANDED);
        }

        getSurfaceRenderer().onSurfaceClosed();
    }

    private void setBottomControlsHeight(int height) {
        if (height == mCachedSheetHeight) return;
        mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(true);
        mBrowserControlsSizer.setBottomControlsHeight(height, 0);
        mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(false);
        mCachedSheetHeight = height;
    }

    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        mExpandedSheetHelper.onSheetExpanded();
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        mExpandedSheetHelper.onSheetCollapsed();
    }

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
        float peekHeightRatio = getPeekHeightRatio();
        if (mSheetController.getSheetState() == SheetState.SCROLLING
                && heightFraction < peekHeightRatio) {
            // Set the content height to zero in advance when user drags/scrolls the sheet down
            // below the peeking state. This helps hide the white patch (blank bottom controls).
            setBottomControlsHeight(0);
        }

        float ratioOfCompletionFromPeekToExpanded =
                (heightFraction - peekHeightRatio) / (1.f - peekHeightRatio);
        setBackgroundColors(ratioOfCompletionFromPeekToExpanded);
        if (0 <= ratioOfCompletionFromPeekToExpanded
                && ratioOfCompletionFromPeekToExpanded <= 1.f) {
            setCornerRadiusPx((int) (ratioOfCompletionFromPeekToExpanded * mMaxCornerRadiusPx));
        }
    }

    private float getPeekHeightRatio() {
        float fullHeight = mSheetContent.getContentView().getMeasuredHeight();
        return mSheetContent.getPeekHeight() / fullHeight;
    }

    void setCornerRadiusPx(int radius) {
        mBackgroundDrawable.mutate();
        mBackgroundDrawable.setCornerRadii(
                new float[] {radius, radius, radius, radius, 0, 0, 0, 0});
    }

    void setBackgroundColors(float ratioOfCompletionFromPeekToExpanded) {
        float colorRatio = 1.0f;
        if (0 <= ratioOfCompletionFromPeekToExpanded
                && ratioOfCompletionFromPeekToExpanded <= 0.5f) {
            colorRatio = 2 * ratioOfCompletionFromPeekToExpanded;
        } else if (ratioOfCompletionFromPeekToExpanded <= 0) {
            colorRatio = 0;
        }
        int surfaceColor = mContext.getColor(R.color.gm3_baseline_surface);
        int surfaceContainerColor = mContext.getColor(R.color.gm3_baseline_surface_container);
        mBackgroundDrawable.setColor(
                ColorUtils.getColorWithOverlay(
                        surfaceContainerColor, surfaceColor, colorRatio, false));
        mSheetContent.setPrivacyCardColor(
                ColorUtils.getColorWithOverlay(
                        surfaceColor, surfaceContainerColor, colorRatio, false));
    }

    @Override
    public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {}

    void destroy() {
        cancelAutoTrigger();
        mBottomUiController.removeObserver(mBottomUiObserver);
        mControlsStateProvider.removeObserver(mBrowserControlsObserver);
        mSheetController.removeObserver(this);
        if (mTabObservable.get() != null) {
            mTabObservable.get().removeObserver(this);
        }
        if (mInMotionSupplier != null) {
            mInMotionSupplier.removeObserver(mInMotionCallback);
        }
        if (mBackPressManager != null && mBackPressHandler != null) {
            mBackPressManager.removeHandler(mBackPressHandler);
        }
        if (mPageInsightsDataLoader != null) {
            mPageInsightsDataLoader.destroy();
        }
    }

    float getCornerRadiusForTesting() {
        float[] radii = mBackgroundDrawable.getCornerRadii();
        assert radii[0] == radii[1] && radii[1] == radii[2] && radii[2] == radii[3];
        return radii[0];
    }

    void setPageInsightsDataLoaderForTesting(PageInsightsDataLoader pageInsightsDataLoader) {
        mPageInsightsDataLoader = pageInsightsDataLoader;
    }

    View getContainerForTesting() {
        return mSheetContainer;
    }

    private PageInsightsSurfaceRenderer getSurfaceRenderer() {
        if (mSurfaceRenderer != null) {
            return mSurfaceRenderer;
        }
        PageInsightsSurfaceScope surfaceScope =
                XSurfaceProcessScopeProvider.getProcessScope()
                        .obtainPageInsightsSurfaceScope(
                                new PageInsightsSurfaceScopeDependencyProviderImpl(mContext));
        mSurfaceRenderer = surfaceScope.provideSurfaceRenderer();
        return mSurfaceRenderer;
    }
}
