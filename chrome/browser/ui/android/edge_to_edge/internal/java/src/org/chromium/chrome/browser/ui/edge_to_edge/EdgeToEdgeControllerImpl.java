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
    private static final String TAG = "E2E_ControllerImpl";

    /** The outermost view in our view hierarchy that is identified with a resource ID. */
    private static final int ROOT_UI_VIEW_ID = android.R.id.content;

    private final @NonNull Activity mActivity;
    private final @NonNull TabSupplierObserver mTabSupplierObserver;
    private final @NonNull TabObserver mTabObserver;

    /** Multiplier to convert from pixels to DPs. */
    private final float mPxToDp;

    private @NonNull EdgeToEdgeOSWrapper mEdgeToEdgeOSWrapper;

    private Tab mCurrentTab;
    private WebContentsObserver mWebContentsObserver;
    private boolean mIsActivityToEdge;
    private Insets mSystemInsets;
    private boolean mDidSetDecorAndListener;

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
    }

    @Override
    @RequiresApi(VERSION_CODES.R)
    public void onTabSwitched(@Nullable Tab tab) {
        if (mCurrentTab != null) mCurrentTab.removeObserver(mTabObserver);
        mCurrentTab = tab;
        if (tab != null) {
            tab.addObserver(mTabObserver);
            if (tab.getWebContents() != null) {
                updateWebContentsObserver(tab);
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
        // TODO(https://crbug.com/1482559#c23) remove this logging by end of '23.
        Log.i(TAG, "E2E_Up Tab '%s'", tab.getTitle());
    }

    /**
     * Conditionally sets the given view ToEdge or ToNormal based on the {@code toEdge} param.
     *
     * @param viewId The ID of the Root UI View, or some view for testing.
     * @param toEdge Whether to draw ToEdge.
     * @param webContents The {@link WebContents} to notify of inset env() changes.
     */
    @RequiresApi(VERSION_CODES.R)
    @SuppressWarnings("WrongConstant") // For WindowInsets.Type on U+
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
            mEdgeToEdgeOSWrapper.setOnApplyWindowInsetsListener(
                    rootView,
                    (view, windowInsets) -> {
                        Insets newInsets =
                                windowInsets.getInsets(
                                        WindowInsets.Type.navigationBars()
                                                + WindowInsets.Type.statusBars());
                        if (!newInsets.equals(mSystemInsets)) {
                            mSystemInsets = newInsets;
                            Log.w(TAG, "System Bar insets changed to: %s", mSystemInsets);
                            // Note that we cannot adjustEdges earlier since we need the system
                            // insets.
                            adjustEdges(mIsActivityToEdge, viewId, webContents);
                        }
                        return windowInsets;
                    });
        } else {
            adjustEdges(toEdge, viewId, webContents);
        }
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
        int bottomInset = toEdge ? 0 : mSystemInsets.bottom;
        mEdgeToEdgeOSWrapper.setPadding(
                mActivity.findViewById(viewId),
                mSystemInsets.left,
                mSystemInsets.top,
                mSystemInsets.right,
                bottomInset);

        // We only make the Nav Bar transparent because it's the only thing we want to draw
        // underneath.
        // TODO(donnd): Use an appropriate background color when not transparent.
        //     For the web we may need to call Blink or some system background color API.
        @ColorInt int navBarColor = toEdge ? Color.TRANSPARENT : Color.BLACK;
        mEdgeToEdgeOSWrapper.setNavigationBarColor(mActivity.getWindow(), navBarColor);

        if (webContents != null) pushInsetsToBlink(toEdge, webContents);
    }

    /**
     * Pushes the current insets to Blink so the page will know how to pad bottom UI.
     *
     * @param toEdge Whether we're drawing all the way to the edge of the screen.
     * @param webContents A {@link WebContents} that leads to a Blink Renderer.
     */
    private void pushInsetsToBlink(boolean toEdge, @NonNull WebContents webContents) {
        // Push the insets back to the webpage if we have one.
        // TODO(https://crbug.com/1475820) Move this work into the nascent
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

    @CallSuper
    @Override
    public void destroy() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }
        if (mCurrentTab != null) mCurrentTab.removeObserver(mTabObserver);
        mTabSupplierObserver.destroy();
    }

    @VisibleForTesting
    public boolean isToEdge() {
        return mIsActivityToEdge;
    }

    public void setOsWrapperForTesting(EdgeToEdgeOSWrapper testOsWrapper) {
        mEdgeToEdgeOSWrapper = testOsWrapper;
    }

    @VisibleForTesting
    @Nullable
    WebContentsObserver getWebContentsObserver() {
        return mWebContentsObserver;
    }

    void setToEdgeForTesting(boolean toEdge) {
        mIsActivityToEdge = toEdge;
    }

    void setSystemInsetsForTesting(Insets systemInsetsForTesting) {
        mSystemInsets = systemInsetsForTesting;
    }
}
