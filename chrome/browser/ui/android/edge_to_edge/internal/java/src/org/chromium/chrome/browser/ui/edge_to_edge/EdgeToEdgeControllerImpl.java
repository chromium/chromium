// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.app.Activity;
import android.graphics.Color;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.CallSuper;
import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;

import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;

/**
 * Controls use of the Android Edge To Edge feature that allows an App to draw benieth the Status
 * and Navigation Bars. For Chrome, we intentend to sometimes draw under the Nav Bar but not the
 * Status Bar.
 */
public class EdgeToEdgeControllerImpl implements EdgeToEdgeController {
    private static final String TAG = "E2EControllerImpl";

    /** The outermost view in our view hierarchy that is identified with a resource ID. */
    private static final int ROOT_UI_VIEW_ID = android.R.id.content;

    private final Activity mActivity;
    private final @NonNull TabSupplierObserver mTabSupplierObserver;
    private final @NonNull EdgeToEdgeOSWrapper mEdgeToEdgeOSWrapper;

    private boolean mToEdge;

    /**
     * Creates an implementation of the EdgeToEdgeController that will use the Android APIs to allow
     * drawing under the System Gesture Navigation Bar.
     * @param activity The activity to update to allow drawing under System Bars.
     * @param tabObservableSupplier A supplier for Tab changes so this implementation can adjust
     *     whether to draw under or not for each page.
     * @param edgeToEdgeOSWrapper An optional wrapper for OS calls for testing etc.
     */
    public EdgeToEdgeControllerImpl(Activity activity,
            ObservableSupplier<Tab> tabObservableSupplier,
            @Nullable EdgeToEdgeOSWrapper edgeToEdgeOSWrapper) {
        mActivity = activity;
        mEdgeToEdgeOSWrapper =
                edgeToEdgeOSWrapper == null ? new EdgeToEdgeOSWrapperImpl() : edgeToEdgeOSWrapper;
        mTabSupplierObserver = new TabSupplierObserver(tabObservableSupplier) {
            @Override
            protected void onObservingDifferentTab(Tab tab) {
                if (android.os.Build.VERSION.SDK_INT < VERSION_CODES.R) return;
                onTabSwitched(tab);
            }
        };
    }

    @Override
    @RequiresApi(VERSION_CODES.R)
    public void onTabSwitched(@Nullable Tab tab) {
        drawToEdge(ROOT_UI_VIEW_ID, shouldDrawToEdge(tab));
    }

    /**
     * Conditionally sets the given view ToEdge or ToNormal based on the {@code toEdge} param.
     * @param viewId The Root UI View, or some view for testing.
     * @param toEdge Whether to draw ToEdge.
     */
    @VisibleForTesting
    @RequiresApi(VERSION_CODES.R)
    @SuppressWarnings("WrongConstant") // For WindowInsets.Type on U+
    void drawToEdge(int viewId, boolean toEdge) {
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
        mEdgeToEdgeOSWrapper.setOnApplyWindowInsetsListener(rootView, (view, windowInsets) -> {
            Insets systemInsets = windowInsets.getInsets(WindowInsets.Type.systemBars());
            int bottomInset = toEdge ? 0 : systemInsets.bottom;
            // Restore the drawing to normal on all edges, except for the bottom (Nav Bar).
            mEdgeToEdgeOSWrapper.setPadding(
                    view, systemInsets.left, systemInsets.top, systemInsets.right, bottomInset);
            return windowInsets;
        });
    }

    /**
     * Decides whether to draw the given Tab ToEdge or not.
     * @param tab The {@link Tab} to be drawn.
     * @return {@code true} if it's OK to draw this Tab under system bars.
     */
    private boolean shouldDrawToEdge(@Nullable Tab tab) {
        boolean isNative = tab == null || tab.isNativePage();
        if (isNative) {
            // Check the flag for ToEdge on all native pages.
            return ChromeFeatureList.sDrawNativeEdgeToEdge.isEnabled();
        } else {
            // Check the flag for ToEdge on all web pages.
            return ChromeFeatureList.sDrawWebEdgeToEdge.isEnabled();
        }
    }

    @VisibleForTesting
    boolean isToEdge() {
        return mToEdge;
    }

    @VisibleForTesting
    void setToEdge(boolean toEdge) {
        mToEdge = toEdge;
    }

    @CallSuper
    @Override
    public void destroy() {
        mTabSupplierObserver.destroy();
    }
}
