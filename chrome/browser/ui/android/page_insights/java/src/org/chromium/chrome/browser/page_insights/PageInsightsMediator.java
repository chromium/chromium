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

import com.google.protobuf.ByteString;

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
    private final Runnable mAutoTriggerRunnable = this::autoTriggerPageInsightsFromTimer;
    private final HashMap<String, Object> mSurfaceRendererContextValues;
    private final ObservableSupplier<Tab> mTabObservable;
    private final Supplier<Profile> mProfileSupplier;
    private final ObservableSupplierImpl<Boolean> mWillHandleBackPressSupplier;
    private final boolean mIsAccessibilityEnabled;

    private PageInsightsDataLoader mPageInsightsDataLoader;
    @Nullable
    private PageInsightsSurfaceRenderer mSurfaceRenderer;
    @Nullable private PageInsightsMetadata mDisplayedMetadata;
    @Nullable private View mCurrentFeedView;
    @Nullable private View mCurrentChildView;
    private boolean mIsShowingChildView;
    private boolean mAutoTriggerReady;
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
            BooleanSupplier isPageInsightsEnabledSupplier,
            Function<NavigationHandle, PageInsightsConfig> pageInsightsConfigProvider) {
        mContext = context;
        mTabObservable = tabObservable;
        mProfileSupplier = profileSupplier;
        mWillHandleBackPressSupplier = new ObservableSupplierImpl<>(false);
        mSheetContent =
                new PageInsightsSheetContent(
                        mContext,
                        layoutView,
                        view -> loadMyActivityUrl(tabObservable),
                        this::handleBackPress,
                        mWillHandleBackPressSupplier,
                        this::handleBottomSheetTap);
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
                        maybeAutoTriggerPageInsights();
                    }
                };
        controlsStateProvider.addObserver(mBrowserControlsObserver);
        bottomSheetController.addObserver(this);
        mBottomUiObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(@SheetState int newState, int reason) {
                onBottomUiStateChanged(newState >= SheetState.PEEK);
            };
        };
        bottomUiController.addObserver(mBottomUiObserver);
        mControlsStateProvider = controlsStateProvider;
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
        mAutoTriggerDelayMs = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END,
                DEFAULT_TRIGGER_DELAY_MS);
        tabObservable.addObserver(
                tab -> {
                    if (tab == null) return;
                    delayStartAutoTrigger(mAutoTriggerDelayMs);
                    tab.addObserver(this);
                });
        if (BackPressManager.isEnabled()) {
            BackPressHandler backPressHandler =
                    bottomSheetController.getBottomSheetBackPressHandler();
            if (backPressHandler != null
                    && backPressManager != null
                    && !backPressManager.has(BackPressHandler.Type.PAGE_INSIGHTS_BOTTOM_SHEET)) {
                backPressManager.addHandler(
                        backPressHandler, BackPressHandler.Type.PAGE_INSIGHTS_BOTTOM_SHEET);
            }
        }
    }

    void initView(View bottomSheetContainer) {
        mSheetContainer = bottomSheetContainer;
        View view = bottomSheetContainer.findViewById(R.id.background);
        mBackgroundDrawable = (GradientDrawable) view.getBackground();
        mMaxCornerRadiusPx = bottomSheetContainer.getResources().getDimensionPixelSize(
                R.dimen.bottom_sheet_corner_radius);
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

    // TabObserver

    private void autoTriggerPageInsightsFromTimer() {
        mAutoTriggerReady = true;
        maybeAutoTriggerPageInsights();
    }

    private void resetAutoTriggerTimer() {
        mAutoTriggerReady = false;
        mHandler.removeCallbacks(mAutoTriggerRunnable);
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        Log.v(TAG, "onPageLoadStarted");
        resetAutoTriggerTimer();
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
    }

    private void delayStartAutoTrigger(long delayMs) {
        if (delayMs > 0) {
            mHandler.postDelayed(mAutoTriggerRunnable, delayMs);
        } else {
            mAutoTriggerRunnable.run();
        }
    }

    private void maybeAutoTriggerPageInsights() {
        if (!mAutoTriggerReady) return;
        if (mSheetContent == mSheetController.getCurrentSheetContent()) {
            Log.v(
                    TAG,
                    "Not auto-triggering because page insights sheet content already being shown.");
            return;
        }
        if (!BrowserControlsUtils.areBrowserControlsOffScreen(mControlsStateProvider)
                && !mIsAccessibilityEnabled) {
            Log.v(
                    TAG,
                    "Not auto-triggering because browser controls are not off screen and a11y is"
                            + " not enabled.");
            return;
        }
        resetAutoTriggerTimer();

        if (!mIsPageInsightsEnabledSupplier.getAsBoolean()) {
            Log.v(TAG, "Not auto-triggering because feature is disabled.");
            return;
        }

        PageInsightsConfig config = mPageInsightsConfigProvider.apply(mCurrentNavigationHandle);
        if (!config.getShouldAutoTrigger()) {
            Log.v(TAG, "Not auto-triggering because auto-triggering is disabled.");
            return;
        }

        Log.v(TAG, "Loading data for auto-peek");
        mPageInsightsDataLoader.loadInsightsData(
                mTabObservable.get().getUrl(),
                config.getShouldAttachGaiaToRequest(),
                metadata -> {
                    mDisplayedMetadata = metadata;
                    boolean hasEnoughConfidence =
                            metadata.getAutoPeekConditions().getConfidence() > MINIMUM_CONFIDENCE;
                    if (hasEnoughConfidence) {
                        Log.v(TAG, "Auto-peeking");
                        openInAutoPeekState(metadata, config.getShouldXsurfaceLog());
                    } else {
                        Log.v(TAG, "Would auto-peek but confidence too low");
                    }
                });
    }

    private void openInAutoPeekState(PageInsightsMetadata metadata, boolean shouldXsurfaceLog) {
        if (shouldXsurfaceLog) {
            getSurfaceRenderer()
                    .onSurfaceCreated(
                            PageInsightsLoggingParametersImpl.create(
                                    mProfileSupplier.get(), metadata));
        }
        initSheetContent(
                metadata,
                /* isPrivacyNoticeRequired= */ shouldXsurfaceLog,
                /* shouldHavePeekState= */ true);
        logPageInsightsEvent(PageInsightsEvent.AUTO_PEEK_TRIGGERED);
        getSurfaceRenderer().onEvent(BOTTOM_SHEET_PEEKING);
        mSheetController.requestShowContent(mSheetContent, true);
    }

    // data
    void launch() {
        resetAutoTriggerTimer();
        mSheetContent.showLoadingIndicator();
        mSheetController.requestShowContent(mSheetContent, true);
        PageInsightsConfig config = mPageInsightsConfigProvider.apply(mCurrentNavigationHandle);
        mPageInsightsDataLoader.loadInsightsData(
                mTabObservable.get().getUrl(),
                config.getShouldAttachGaiaToRequest(),
                metadata -> {
                    mDisplayedMetadata = metadata;
                    if (config.getShouldXsurfaceLog()) {
                        getSurfaceRenderer()
                                .onSurfaceCreated(
                                        PageInsightsLoggingParametersImpl.create(
                                                mProfileSupplier.get(), metadata));
                    }
                    initSheetContent(
                            metadata,
                            /* isPrivacyNoticeRequired= */ config.getShouldXsurfaceLog(),
                            /* shouldHavePeekState= */ false);
                    setBackgroundColors(/* ratioOfCompletionFromPeekToExpanded */ 1.0f);
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
        return getSurfaceRenderer().render(
                elementsOutput.toByteArray(), mSurfaceRendererContextValues);
    }

    private void changeToChildPage(int id) {
        if (mDisplayedMetadata == null) {
            return;
        }

        for (int i = 0; i < mDisplayedMetadata.getPagesCount(); i++) {
            Page currPage = mDisplayedMetadata.getPages(i);
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
            setBackgroundColors(/* ratioOfCompletionFromPeekToExpanded */ .0f);
            logPageInsightsEvent(PageInsightsEvent.STATE_PEEK);
            // We don't log peek state to XSurface here, as its BOTTOM_SHEET_PEEKING event is only
            // intended for when the feature initially auto-peeks.
        } else if (newState == SheetState.FULL) {
            mWillHandleBackPressSupplier.set(true);
            setBackgroundColors(/* ratioOfCompletionFromPeekToExpanded */ 1.0f);
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
                && heightFraction + 0.01f < peekHeightRatio) {
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
        float fullHeight = mSheetContent.getActualFullHeightRatio() * mSheetContainer.getHeight();
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
        resetAutoTriggerTimer();
        mBottomUiController.removeObserver(mBottomUiObserver);
    }

    float getCornerRadiusForTesting() {
        float[] radii = mBackgroundDrawable.getCornerRadii();
        assert radii[0] == radii[1] && radii[1] == radii[2] && radii[2] == radii[3];
        return radii[0];
    }

    void setAutoTriggerReadyForTesting() {
        mHandler.removeCallbacks(mAutoTriggerRunnable);
        mAutoTriggerReady = true;
    }

    boolean getAutoTriggerReadyForTesting() {
        return mAutoTriggerReady;
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
                XSurfaceProcessScopeProvider.getProcessScope().obtainPageInsightsSurfaceScope(
                        new PageInsightsSurfaceScopeDependencyProviderImpl(mContext));
        mSurfaceRenderer = surfaceScope.provideSurfaceRenderer();
        return mSurfaceRenderer;
    }
}
