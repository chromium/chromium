// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.shouldDrawToEdge;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetObserver.WindowInsetsConsumer;
import org.chromium.components.browser_ui.widget.InsetObserverSupplier;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * Controls use of the Android Edge To Edge feature that allows an App to draw benieth the Status
 * and Navigation Bars. For Chrome, we intentend to sometimes draw under the Nav Bar but not the
 * Status Bar.
 */
@RequiresApi(VERSION_CODES.R)
public class EdgeToEdgeControllerImpl
        implements EdgeToEdgeController, BrowserControlsStateProvider.Observer {
    private static final String TAG = "E2E_ControllerImpl";

    /** The outermost view in our view hierarchy that is identified with a resource ID. */
    private static final int ROOT_UI_VIEW_ID = android.R.id.content;

    private final @NonNull Activity mActivity;
    private final @NonNull WindowAndroid mWindowAndroid;
    private final @NonNull TabSupplierObserver mTabSupplierObserver;
    private final ObserverList<EdgeToEdgePadAdjuster> mPadAdjusters = new ObserverList<>();
    private final ObserverList<ChangeObserver> mEdgeChangeObservers = new ObserverList<>();
    private final @NonNull TabObserver mTabObserver;
    private final @Nullable TotallyEdgeToEdge mTotallyEdgeToEdge;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** Multiplier to convert from pixels to DPs. */
    private final float mPxToDp;

    private @NonNull EdgeToEdgeOSWrapper mEdgeToEdgeOSWrapper;

    private Tab mCurrentTab;
    private WebContentsObserver mWebContentsObserver;
    private boolean mIsActivityToEdge;
    private boolean mDidSetDecorAndListener;
    private InsetObserver mInsetObserver;
    private @Nullable Insets mSystemInsets;
    private @Nullable Insets mKeyboardInsets;
    private @Nullable WindowInsetsConsumer mWindowInsetsConsumer;
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
     * @param edgeToEdgeOSWrapper An optional wrapper for OS calls for testing etc.
     * @param browserControlsStateProvider Provides the state of the BrowserControls for Totally
     *     Edge to Edge.
     */
    public EdgeToEdgeControllerImpl(
            Activity activity,
            WindowAndroid windowAndroid,
            ObservableSupplier<Tab> tabObservableSupplier,
            @Nullable EdgeToEdgeOSWrapper edgeToEdgeOSWrapper,
            BrowserControlsStateProvider browserControlsStateProvider) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mEdgeToEdgeOSWrapper =
                edgeToEdgeOSWrapper == null ? new EdgeToEdgeOSWrapperImpl() : edgeToEdgeOSWrapper;
        mPxToDp = 1.f / mActivity.getResources().getDisplayMetrics().density;
        mTabSupplierObserver =
                new TabSupplierObserver(tabObservableSupplier) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab) {
                        onTabSwitched(tab);
                    }
                };
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onWebContentsSwapped(
                            Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                        updateWebContentsObserver(tab);
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        assert tab.getWebContents() != null
                                : "onContentChanged called on tab w/o WebContents: "
                                        + tab.getTitle();
                        updateWebContentsObserver(tab);
                    }
                };
        mTotallyEdgeToEdge =
                TotallyEdgeToEdge.isEnabled()
                        ? new TotallyEdgeToEdge(
                                browserControlsStateProvider,
                                () ->
                                        maybeDrawToEdge(
                                                ROOT_UI_VIEW_ID,
                                                mCurrentTab == null
                                                        ? null
                                                        : mCurrentTab.getWebContents()))
                        : null;
        mInsetObserver = InsetObserverSupplier.getValueOrNullFrom(mWindowAndroid);
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);
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
        maybeDrawToEdge(ROOT_UI_VIEW_ID, tab == null ? null : tab.getWebContents());
    }

    @Override
    public void registerAdjuster(EdgeToEdgePadAdjuster adjuster) {
        mPadAdjusters.addObserver(adjuster);
        if (mSystemInsets != null) {
            boolean shouldPad = shouldPadAdjusters();
            adjuster.overrideBottomInset(
                    shouldPad ? mSystemInsets.bottom : 0,
                    shouldPad && !mBottomControlsAreVisible ? mSystemInsets.bottom : 0);
        }
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
        return mSystemInsets == null || !isToEdge()
                ? 0
                : (int) Math.ceil(mSystemInsets.bottom * mPxToDp);
    }

    @Override
    public boolean isToEdge() {
        return mIsActivityToEdge;
    }

    @Override
    public boolean isEdgeToEdgeActive() {
        return mDidSetDecorAndListener;
    }

    // BrowserControlsStateProvider.Observer

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate) {
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
    }

    private void updateBrowserControlsVisibility(boolean visible) {
        if (mBottomControlsAreVisible == visible || mSystemInsets == null) {
            return;
        }
        mBottomControlsAreVisible = visible;
        for (var adjuster : mPadAdjusters) {
            boolean shouldPad = shouldPadAdjusters();
            adjuster.overrideBottomInset(
                    shouldPad ? mSystemInsets.bottom : 0,
                    shouldPad && !mBottomControlsAreVisible ? mSystemInsets.bottom : 0);
        }
    }

    /**
     * Updates our private WebContentsObserver member to point to the given Tab's WebContents.
     * Destroys any previous member.
     *
     * @param tab The {@link Tab} whose {@link WebContents} we want to observe.
     */
    private void updateWebContentsObserver(Tab tab) {
        if (mWebContentsObserver != null) mWebContentsObserver.destroy();
        mWebContentsObserver =
                new WebContentsObserver(tab.getWebContents()) {
                    @Override
                    public void viewportFitChanged(@WebContentsObserver.ViewportFitType int value) {
                        maybeDrawToEdge(ROOT_UI_VIEW_ID, value, tab.getWebContents());
                    }
                };
        // TODO(https://crbug.com/1482559#c23) remove this logging by end of '23.
        Log.i(TAG, "E2E_Up Tab '%s'", tab.getTitle());
    }

    /**
     * Conditionally draws the given View ToEdge or ToNormal based on {@link
     * EdgeToEdgeUtils#shouldDrawToEdge(Tab)}
     *
     * @param viewId The ID of the Root UI View.
     * @param webContents The {@link WebContents} to notify of inset env() changes.
     */
    private void maybeDrawToEdge(int viewId, @Nullable WebContents webContents) {
        Log.v(TAG, "maybeDrawToEdge? totally: %s", totallyToEdge());
        drawToEdge(viewId, shouldDrawToEdge(mCurrentTab) || totallyToEdge(), webContents);
    }

    /**
     * Conditionally draws the given View ToEdge or ToNormal based on {@link
     * EdgeToEdgeUtils#shouldDrawToEdge(Tab, int)}.
     *
     * @param viewId The ID of the Root UI View.
     * @param value A new {@link WebContentsObserver.ViewportFitType} value being applied now.
     * @param webContents The {@link WebContents} to notify of inset env() changes.
     */
    private void maybeDrawToEdge(
            int viewId,
            @WebContentsObserver.ViewportFitType int value,
            @Nullable WebContents webContents) {
        Log.v(TAG, "maybeDrawToEdge? totally: %s", totallyToEdge());
        drawToEdge(viewId, shouldDrawToEdge(mCurrentTab, value) || totallyToEdge(), webContents);
    }

    /**
     * @return if we should draw totally to the edge now.
     */
    private boolean totallyToEdge() {
        return mTotallyEdgeToEdge != null && mTotallyEdgeToEdge.shouldDrawToEdge();
    }

    /**
     * Conditionally draws the given View ToEdge or ToNormal based on the {@code toEdge} param.
     *
     * @param viewId The ID of the Root UI View.
     * @param toEdge Whether to draw ToEdge.
     * @param webContents The {@link WebContents} to notify of inset env() changes.
     */
    private void drawToEdge(int viewId, boolean toEdge, @Nullable WebContents webContents) {
        if (toEdge == mIsActivityToEdge) return;

        mIsActivityToEdge = toEdge;
        Log.v(TAG, "Switching %s", (toEdge ? "ToEdge" : "ToNormal"));
        View rootView = mActivity.findViewById(viewId);
        assert rootView != null : "Root view for Edge To Edge not found!";

        // Setup the basic enabling of the Edge to Edge Android Feature.
        // Sets up this window to open up System edges to be drawn underneath.
        if (toEdge && mSystemInsets == null && !mDidSetDecorAndListener) {
            mDidSetDecorAndListener = true;
            mEdgeToEdgeOSWrapper.setDecorFitsSystemWindows(mActivity.getWindow(), false);
            mWindowInsetsConsumer =
                    (view, windowInsets) -> handleWindowInsets(windowInsets, viewId, webContents);
            if (EdgeToEdgeUtils.isInsetsManagementEnabled()) {
                mInsetObserver.addInsetsConsumer(mWindowInsetsConsumer);
            } else {
                mEdgeToEdgeOSWrapper.setOnApplyWindowInsetsListener(
                        rootView, mWindowInsetsConsumer);
            }
        } else if (mSystemInsets != null) {
            // It's possible for toEdge to change more than once prior to the first time
            // #handleWindowInsets is called. #handleWindowInsets will call #adjustEdges using
            // the current mIsActivityToEdge once insets are available.
            adjustEdges(toEdge, viewId, webContents);
        }
    }

    private WindowInsetsCompat handleWindowInsets(
            @NonNull WindowInsetsCompat windowInsets,
            int viewId,
            @Nullable WebContents webContents) {
        Insets newInsets =
                windowInsets.getInsets(
                        WindowInsetsCompat.Type.navigationBars()
                                + WindowInsetsCompat.Type.statusBars());
        Insets newKeyboardInsets = windowInsets.getInsets(WindowInsetsCompat.Type.ime());

        if (!newInsets.equals(mSystemInsets) || !newKeyboardInsets.equals(mKeyboardInsets)) {
            mSystemInsets = newInsets;
            mKeyboardInsets = newKeyboardInsets;

            // When a foldable goes to/from tablet mode we must reassess.
            // TODO(https://crbug.com/325356134) Find a cleaner check and remedy.
            mIsActivityToEdge =
                    mIsActivityToEdge
                            && EdgeToEdgeControllerFactory.isSupportedConfiguration(mActivity);
            // Note that we cannot adjustEdges earlier since we need the system
            // insets.
            adjustEdges(mIsActivityToEdge, viewId, webContents);
        }
        return windowInsets;
    }

    /**
     * The {@link EdgeToEdgePadAdjuster}s should only be padded with an extra bottom inset if the
     * activity is currently in edge-to-edge, and if the adjusters aren't already positioned above
     * the system insets due to the keyboard or the bottom controls being visible.
     */
    private boolean shouldPadAdjusters() {
        boolean keyboardIsVisible = mKeyboardInsets != null && mKeyboardInsets.bottom > 0;
        return mIsActivityToEdge && !keyboardIsVisible;
    }

    /**
     * Adjusts whether the given view draws ToEdge or ToNormal. The ability to draw under System
     * Bars should have already been set. This method only sets the padding of the view and
     * transparency of the Nav Bar, etc.
     *
     * @param toEdge Whether to adjust the drawing environment ToEdge.
     * @param viewId The ID of the view to adjust.
     * @param webContents A {@link WebContents} to notify Blink of the adjusted insets.
     */
    private void adjustEdges(boolean toEdge, int viewId, @Nullable WebContents webContents) {
        assert mSystemInsets != null : "Trying to adjustToEdge without mSystemInsets!";

        // Adjust the bottom padding to reflect whether ToEdge or ToNormal for the Gesture Nav Bar.
        // All the other edges need to be padded to prevent drawing under an edge that we
        // don't want drawn ToEdge (e.g. the Status Bar).
        int bottomPadding = toEdge ? 0 : mSystemInsets.bottom;
        if (mKeyboardInsets != null && mKeyboardInsets.bottom > bottomPadding) {
            // If the keyboard is showing, change the bottom padding to account for the keyboard.
            // Clear the bottom inset used for the adjusters, since there are no missing bottom
            // system bars above the keyboard to compensate for.
            bottomPadding = mKeyboardInsets.bottom;
        }

        mEdgeToEdgeOSWrapper.setPadding(
                mActivity.findViewById(viewId),
                mSystemInsets.left,
                mSystemInsets.top,
                mSystemInsets.right,
                bottomPadding);

        for (var adjuster : mPadAdjusters) {
            boolean shouldPad = shouldPadAdjusters();
            adjuster.overrideBottomInset(
                    shouldPad ? mSystemInsets.bottom : 0,
                    shouldPad && !mBottomControlsAreVisible ? mSystemInsets.bottom : 0);
        }
        for (var observer : mEdgeChangeObservers) {
            observer.onToEdgeChange(isToEdge() ? mSystemInsets.bottom : 0);
        }

        if (EdgeToEdgeUtils.isInsetsManagementEnabled()) {
            int bottomInsetOnSaveArea = toEdge ? mSystemInsets.bottom : 0;
            mInsetObserver.updateBottomInsetForEdgeToEdge(bottomInsetOnSaveArea);
        } else if (webContents != null) {
            pushInsetsToBlink(toEdge, webContents);
        }
    }

    /**
     * Pushes the current insets to Blink so the page will know how to pad bottom UI.
     *
     * @param toEdge Whether we're drawing all the way to the edge of the screen.
     * @param webContents A {@link WebContents} that leads to a Blink Renderer.
     */
    private void pushInsetsToBlink(boolean toEdge, @NonNull WebContents webContents) {
        // Push the insets back to the webpage if we have one.
        // TODO(crbug.com/40279791) Move this work into the nascent
        // SafeAreaInsetsTracker.
        assert mSystemInsets != null : "Error, trying to notify Blink without system insets set";
        Rect insetsRect = new Rect(0, 0, 0, toEdge ? scale(mSystemInsets.bottom) : 0);
        Log.v(TAG, "Pushing back insets to Blink %s", insetsRect);
        webContents.setDisplayCutoutSafeArea(insetsRect);
    }

    /**
     * Conditionally sets the given view ToEdge or ToNormal based on the {@code toEdge} param.
     *
     * @param viewId The Root UI View, or some view for testing.
     * @param toEdge Whether to draw ToEdge.
     */
    @VisibleForTesting
    void drawToEdge(int viewId, boolean toEdge) {
        drawToEdge(viewId, toEdge, null);
    }

    /**
     * @return the value of the pixel input when scaled back to density-independent pixels.
     */
    private int scale(@Px int unscaledValuePx) {
        return (int) Math.ceil(unscaledValuePx * mPxToDp);
    }

    @CallSuper
    @Override
    public void destroy() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }
        if (mCurrentTab != null) mCurrentTab.removeObserver(mTabObserver);
        mTabSupplierObserver.destroy();
        if (mTotallyEdgeToEdge != null) mTotallyEdgeToEdge.destroy();
        if (mInsetObserver != null) {
            mInsetObserver.removeInsetsConsumer(mWindowInsetsConsumer);
            mInsetObserver = null;
        }
        if (mBrowserControlsStateProvider != null) {
            mBrowserControlsStateProvider.removeObserver(this);
        }
    }

    public void setOsWrapperForTesting(EdgeToEdgeOSWrapper testOsWrapper) {
        mEdgeToEdgeOSWrapper = testOsWrapper;
    }

    @VisibleForTesting
    @Nullable
    WebContentsObserver getWebContentsObserver() {
        return mWebContentsObserver;
    }

    public void setToEdgeForTesting(boolean toEdge) {
        mIsActivityToEdge = toEdge;
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
}
