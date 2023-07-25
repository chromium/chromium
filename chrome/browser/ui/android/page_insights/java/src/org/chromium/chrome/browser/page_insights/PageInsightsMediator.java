// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.text.format.DateUtils;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.url.GURL;

import java.util.function.BooleanSupplier;

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
    private static final long DEFAULT_TRIGGER_DELAY_MS = DateUtils.SECOND_IN_MILLIS * 60;
    private static final double MINIMUM_CONFIDENCE = 50;

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

    private final BooleanSupplier mIsPageInsightsHubEnabled;
    private final Handler mHandler;
    private final Runnable mAutoTriggerRunnable = this::autoTriggerPageInsightsFromTimer;

    private PageInsightsDataLoader mPageInsightsDataLoader;

    private boolean mAutoTriggerReady;

    // Caches the sheet height at the current state. Avoids the repeated call to resize the content
    // if the size hasn't changed since.
    private int mCachedSheetHeight;

    // Whether the sheet was hidden due to another bottom sheet UI, and needs to be restored
    // when notified when the UI was closed.
    private boolean mShouldRestore;

    public PageInsightsMediator(Context context, ObservableSupplier<Tab> tabObservable,
            ManagedBottomSheetController bottomSheetController,
            BottomSheetController bottomUiController, ExpandedSheetHelper expandedSheetHelper,
            BrowserControlsStateProvider controlsStateProvider,
            BrowserControlsSizer browserControlsSizer, BooleanSupplier isPageInsightsHubEnabled) {
        mContext = context;
        mSheetContent = new PageInsightsSheetContent(mContext);
        mSheetController = bottomSheetController;
        mBottomUiController = bottomUiController;
        tabObservable.addObserver(tab -> {
            if (tab != null) {
                tab.addObserver(this);
            }
        });
        mExpandedSheetHelper = expandedSheetHelper;
        mHandler = new Handler(Looper.getMainLooper());
        mBrowserControlsSizer = browserControlsSizer;
        mBrowserControlsObserver = new BrowserControlsStateProvider.Observer() {
            @Override
            public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                    int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
                bottomSheetController.setBrowserControlsHiddenRatio(
                        controlsStateProvider.getBrowserControlHiddenRatio());
                if (mAutoTriggerReady) maybeAutoTriggerPageInsights();
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
        mIsPageInsightsHubEnabled = isPageInsightsHubEnabled;
        mPageInsightsDataLoader = new PageInsightsDataLoader();
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
        resetAutoTriggerTimer();
        if (mSheetContent == mSheetController.getCurrentSheetContent()) {
            mSheetController.hideContent(mSheetContent, false);
        }
    }

    @Override
    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
        // onPageLoadFinished is not suitable as it is not fired when going back to a cached page.
        if (!toDifferentDocument) return;
        resetAutoTriggerTimer();
        // TODO(ggeorgiana): read duration from flag
        mHandler.postDelayed(mAutoTriggerRunnable, DEFAULT_TRIGGER_DELAY_MS);
    }

    private void maybeAutoTriggerPageInsights() {
        if (!mIsPageInsightsHubEnabled.getAsBoolean()
                || !BrowserControlsUtils.areBrowserControlsOffScreen(mControlsStateProvider)
                || mSheetContent == mSheetController.getCurrentSheetContent()
                || !mAutoTriggerReady) {
            return;
        }
        boolean hasEnoughConfidence =
                mPageInsightsDataLoader.loadInsightsData().getConfidence() * 100
                > MINIMUM_CONFIDENCE;
        if (hasEnoughConfidence) {
            requestShowContent();
            resetAutoTriggerTimer();
        }
    }

    void requestShowContent() {
        mSheetController.requestShowContent(mSheetContent, true);
    }

    // BottomSheetObserver

    @Override
    public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
        if (newState == SheetState.HIDDEN) resetAutoTriggerTimer();
        if (newState == SheetState.HIDDEN || newState == SheetState.PEEK) {
            setBottomControlsHeight(mSheetController.getCurrentOffset());
        }
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

        float ratio = (heightFraction - peekHeightRatio) / (1.f - peekHeightRatio);
        if (0 <= ratio && ratio <= 1.f) setCornerRadiusPx((int) (ratio * mMaxCornerRadiusPx));
    }

    private float getPeekHeightRatio() {
        float fullHeight = mSheetContent.getFullHeightRatio() * mSheetContainer.getHeight();
        return mSheetContent.getPeekHeight() / fullHeight;
    }

    void setCornerRadiusPx(int radius) {
        mBackgroundDrawable.mutate();
        mBackgroundDrawable.setCornerRadii(
                new float[] {radius, radius, radius, radius, 0, 0, 0, 0});
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

    void setPageInsightsDataLoaderForTesting(PageInsightsDataLoader pageInsightsDataLoader) {
        mPageInsightsDataLoader = pageInsightsDataLoader;
    }

    View getContainerForTesting() {
        return mSheetContainer;
    }
}
