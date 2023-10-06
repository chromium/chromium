// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.CallSuper;
import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;

import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController.SafeAreaInsetsTracker;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * Controls use of the Android Edge To Edge feature that allows an App to draw benieth the Status
 * and Navigation Bars. For Chrome, we intentend to sometimes draw under the Nav Bar but not the
 * Status Bar.
 */
public class EdgeToEdgeControllerImpl implements EdgeToEdgeController {
    private static final String TAG = "E2EControllerImpl";

    /** The outermost view in our view hierarchy that is identified with a resource ID. */
    private static final int ROOT_UI_VIEW_ID = android.R.id.content;

    private final @NonNull Activity mActivity;
    private final @NonNull TabSupplierObserver mTabSupplierObserver;
    private final @NonNull EdgeToEdgeOSWrapper mEdgeToEdgeOSWrapper;

    /** Multiplier to convert from pixels to DPs. */
    private final float mPxToDp;

    private Tab mRecentTab;
    private TabObserver mTabObserver;
    private WebContentsObserver mWebContentsObserver;
    private boolean mToEdge;

    /**
     * Creates an implementation of the EdgeToEdgeController that will use the Android APIs to allow
     * drawing under the System Gesture Navigation Bar.
     *
     * @param activity The activity to update to allow drawing under System Bars.
     * @param tabObservableSupplier A supplier for Tab changes so this implementation can adjust
     *     whether to draw under or not for each page.
     * @param edgeToEdgeOSWrapper An optional wrapper for OS calls for testing etc.
     */
    @RequiresApi(VERSION_CODES.R)
    public EdgeToEdgeControllerImpl(
            Activity activity,
            ObservableSupplier<Tab> tabObservableSupplier,
            @Nullable EdgeToEdgeOSWrapper edgeToEdgeOSWrapper) {
        mActivity = activity;
        mEdgeToEdgeOSWrapper =
                edgeToEdgeOSWrapper == null ? new EdgeToEdgeOSWrapperImpl() : edgeToEdgeOSWrapper;
        mPxToDp = 1.f / mActivity.getResources().getDisplayMetrics().density;
        mTabSupplierObserver = new TabSupplierObserver(tabObservableSupplier) {
            @Override
            protected void onObservingDifferentTab(Tab tab) {
                onTabSwitched(tab);
            }
        };
    }

    @Override
    @RequiresApi(VERSION_CODES.R)
    public void onTabSwitched(@Nullable Tab tab) {
        removeAnyTabObserver();
        mRecentTab = tab;
        if (tab != null) {
            if (mTabObserver != null) {
                tab.removeObserver(mTabObserver);
                mTabObserver = null;
            }
            if (tab.getWebContents() != null) {
                updateWebContentsObserver(tab);
                // Also disconnect the WebContentsObserver when this tab switches its WebContents.
                mTabObserver =
                        new EmptyTabObserver() {
                            @Override
                            public void onWebContentsSwapped(
                                    Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                                updateWebContentsObserver(tab);
                            }
                        };
                tab.addObserver(mTabObserver);
            }
        }

        boolean shouldDrawToEdge = alwaysDrawToEdgeForTabKind(tab);
        if (!shouldDrawToEdge && tab != null) shouldDrawToEdge = getWasViewportFitCover(tab);
        drawToEdge(ROOT_UI_VIEW_ID, shouldDrawToEdge, tab == null ? null : tab.getWebContents());
    }

    /**
     * Updates our private WebContentsObserver member to point to the given Tab's WebContents.
     * Destroys any previous member.
     *
     * @param tab The {@link Tab} whose {@link WebContents} we want to observe.
     */
    @RequiresApi(VERSION_CODES.R)
    private void updateWebContentsObserver(Tab tab) {
        if (mWebContentsObserver != null) mWebContentsObserver.destroy();
        mWebContentsObserver =
                new WebContentsObserver(tab.getWebContents()) {
                    @Override
                    public void viewportFitChanged(@WebContentsObserver.ViewportFitType int value) {
                        boolean shouldDrawToEdge = alwaysDrawToEdgeForTabKind(tab);
                        if (value == ViewportFit.COVER
                                || value == ViewportFit.COVER_FORCED_BY_USER_AGENT) {
                            shouldDrawToEdge = true;
                        }
                        drawToEdge(ROOT_UI_VIEW_ID, shouldDrawToEdge, tab.getWebContents());
                    }
                };
    }

    /**
     * Conditionally sets the given view ToEdge or ToNormal based on the {@code toEdge} param.
     *
     * @param viewId The Root UI View, or some view for testing.
     * @param toEdge Whether to draw ToEdge.
     * @param webContents The {@link WebContents} to notify of inset env() changes.
     */
    @RequiresApi(VERSION_CODES.R)
    @SuppressWarnings("WrongConstant") // For WindowInsets.Type on U+
    private void drawToEdge(int viewId, boolean toEdge, @Nullable WebContents webContents) {
        if (toEdge == mToEdge) return;
        mToEdge = toEdge;

        Log.i(TAG, "Switching " + (toEdge ? "ToEdge" : "ToNormal"));
        View rootView = mActivity.findViewById(viewId);
        assert rootView != null : "Root view for Edge To Edge not found!";

        // Setup the basic enabling / disabling of the Edge to Edge Android Feature.
        // Sets up this window to open up all edges to be drawn underneath, or not.
        // Note that fitInsideSystemWindows == true means we do NOT draw under the Bars, rather
        // we fit within them. So a value of false is needed to activate ToEdge.
        boolean fitInsideSystemWindows = !toEdge;
        mEdgeToEdgeOSWrapper.setDecorFitsSystemWindows(
                mActivity.getWindow(), fitInsideSystemWindows);

        // We only make the Nav Bar transparent because it's the only thing we want to draw
        // underneath.
        // TODO(donnd): Use an appropriate background color when not transparent.
        //     For the web we may need to call Blink or some system background color API.
        @ColorInt
        int navBarColor = toEdge ? Color.TRANSPARENT : Color.BLACK;
        mEdgeToEdgeOSWrapper.setNavigationBarColor(mActivity.getWindow(), navBarColor);

        // Now fix all the edges other than the bottom Gesture Nav Bar by insetting with padding, or
        // cancelling the previous padding when we adjust back ToNormal. This keeps the
        // setDecorFitsSystemWindows from actually drawing under the edges that we don't want ToEdge
        // (e.g. Status Bar). When moving back ToNormal we need to clear the padding that we added
        // to prevent drawing under the Status Bar otherwise we'll be inset too much at the top of
        // the screen.
        mEdgeToEdgeOSWrapper.setOnApplyWindowInsetsListener(
                rootView,
                (view, windowInsets) -> {
                    Insets systemInsets = windowInsets.getInsets(WindowInsets.Type.systemBars());
                    int bottomInset = toEdge ? 0 : systemInsets.bottom;
                    // Restore the drawing to normal on all edges, except for the bottom (Nav Bar).
                    mEdgeToEdgeOSWrapper.setPadding(
                            view,
                            systemInsets.left,
                            systemInsets.top,
                            systemInsets.right,
                            bottomInset);

                    // Push the insets back to the webpage if we have one.
                    // TODO(https://crbug.com/1475820) Move this work into the nascent
                    // SafeAreaInsetsTracker.
                    if (webContents != null) {
                        Rect insetsRect = new Rect(0, 0, 0, scale(systemInsets.bottom));
                        webContents.setDisplayCutoutSafeArea(insetsRect);
                    }

                    return windowInsets;
                });
    }

    /**
     * Conditionally sets the given view ToEdge or ToNormal based on the {@code toEdge} param.
     *
     * @param viewId The Root UI View, or some view for testing.
     * @param toEdge Whether to draw ToEdge.
     */
    @VisibleForTesting
    @RequiresApi(VERSION_CODES.R)
    void drawToEdge(int viewId, boolean toEdge) {
        drawToEdge(viewId, toEdge, null);
    }

    /**
     * @return the value of the pixel input when scaled back to density-independent pixels.
     */
    private int scale(@Px int unscaledValuePx) {
        return (int) Math.ceil(unscaledValuePx * mPxToDp);
    }

    /**
     * Decides whether to draw the given Tab ToEdge or not.
     *
     * @param tab The {@link Tab} to be drawn.
     * @return {@code true} if it's OK to draw this Tab under system bars.
     */
    private boolean alwaysDrawToEdgeForTabKind(@Nullable Tab tab) {
        boolean isNative = tab == null || tab.isNativePage();
        if (isNative) {
            // Check the flag for ToEdge on all native pages.
            return ChromeFeatureList.sDrawNativeEdgeToEdge.isEnabled();
        }
        return ChromeFeatureList.sDrawWebEdgeToEdge.isEnabled();
    }

    /**
     * Returns whether the given Tab has a web page that was already rendered with
     * viewport-fit=cover.
     */
    private boolean getWasViewportFitCover(@NonNull Tab tab) {
        assert tab != null;
        SafeAreaInsetsTracker safeAreaInsetsTracker =
                DisplayCutoutController.getSafeAreaInsetsTracker(tab);
        return safeAreaInsetsTracker == null ? false : safeAreaInsetsTracker.isViewportFitCover();
    }

    /** Removes any existing TabObserver tracked by private members for the Tab and TabObserver. */
    private void removeAnyTabObserver() {
        if (mRecentTab != null && mTabObserver != null) {
            mRecentTab.removeObserver(mTabObserver);
            mTabObserver = null;
        }
    }

    @CallSuper
    @Override
    public void destroy() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }
        removeAnyTabObserver();
        mTabSupplierObserver.destroy();
    }

    @VisibleForTesting
    @Nullable
    WebContentsObserver getWebContentsObserver() {
        return mWebContentsObserver;
    }

    @VisibleForTesting
    boolean isToEdge() {
        return mToEdge;
    }

    void setToEdgeForTesting(boolean toEdge) {
        mToEdge = toEdge;
    }

}
