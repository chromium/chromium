// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.shouldDrawToEdge;

import android.app.Activity;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetObserver.WindowInsetsConsumer;
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
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final LayoutManager mLayoutManager;

    /** Multiplier to convert from pixels to DPs. */
    private final float mPxToDp;

    private @NonNull EdgeToEdgeOSWrapper mEdgeToEdgeOSWrapper;

    private Tab mCurrentTab;
    private WebContentsObserver mWebContentsObserver;

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

    private InsetObserver mInsetObserver;
    private @NonNull Insets mSystemInsets;
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
     * @param layoutManager The {@link LayoutManager} for checking the active layout type.
     */
    public EdgeToEdgeControllerImpl(
            Activity activity,
            WindowAndroid windowAndroid,
            ObservableSupplier<Tab> tabObservableSupplier,
            @Nullable EdgeToEdgeOSWrapper edgeToEdgeOSWrapper,
            BrowserControlsStateProvider browserControlsStateProvider,
            LayoutManager layoutManager) {
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
        mInsetObserver = mWindowAndroid.getInsetObserver();
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);
        mLayoutManager = layoutManager;

        mWindowInsetsConsumer = this::handleWindowInsets;
        mInsetObserver.addInsetsConsumer(mWindowInsetsConsumer);

        assert mInsetObserver.getLastRawWindowInsets() != null
                : "The inset observer should have non-null insets by the time the"
                        + " EdgeToEdgeControllerImpl is initialized.";
        mSystemInsets = getSystemInsets(mInsetObserver.getLastRawWindowInsets());
        View contentView = mActivity.findViewById(ROOT_UI_VIEW_ID);
        assert contentView != null : "Root view for Edge To Edge not found!";

        adjustEdges(mIsDrawingToEdge, contentView);

        mEdgeToEdgeOSWrapper.setDecorFitsSystemWindows(mActivity.getWindow(), false);
        drawToEdge(EdgeToEdgeUtils.isPageOptedIntoEdgeToEdge(mCurrentTab));
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

        drawToEdge(EdgeToEdgeUtils.isPageOptedIntoEdgeToEdge(mCurrentTab));
    }

    @Override
    public void registerAdjuster(EdgeToEdgePadAdjuster adjuster) {
        mPadAdjusters.addObserver(adjuster);
        boolean shouldPad = shouldPadAdjusters();
        adjuster.overrideBottomInset(
                shouldPad ? mSystemInsets.bottom : 0,
                shouldPad && !mBottomControlsAreVisible ? mSystemInsets.bottom : 0);
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
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
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
    }

    private void updateBrowserControlsVisibility(boolean visible) {
        if (mBottomControlsAreVisible == visible) {
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
                        drawToEdge(EdgeToEdgeUtils.isPageOptedIntoEdgeToEdge(mCurrentTab, value));
                    }
                };
        // TODO(https://crbug.com/1482559#c23) remove this logging by end of '23.
        Log.i(TAG, "E2E_Up Tab '%s'", tab.getTitle());
    }

    /**
     * Conditionally draws the given View ToEdge or ToNormal based on the {@code toEdge} param.
     *
     * @param pageOptedIntoEdgeToEdge Whether the page is opted into edge-to-edge.
     */
    @VisibleForTesting
    void drawToEdge(boolean pageOptedIntoEdgeToEdge) {
        boolean shouldDrawToEdge =
                shouldDrawToEdge(
                        pageOptedIntoEdgeToEdge,
                        mLayoutManager.getActiveLayoutType(),
                        mSystemInsets.bottom);

        boolean changedPageOptedIn = pageOptedIntoEdgeToEdge != mIsPageOptedIntoEdgeToEdge;
        boolean changedDrawToEdge = shouldDrawToEdge != mIsDrawingToEdge;
        mIsPageOptedIntoEdgeToEdge = pageOptedIntoEdgeToEdge;
        mIsDrawingToEdge = shouldDrawToEdge;

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

            View contentView = mActivity.findViewById(ROOT_UI_VIEW_ID);
            assert contentView != null : "Root view for Edge To Edge not found!";

            adjustEdges(mIsDrawingToEdge, contentView);
        }

        // Notify observers if either opt-in status or toEdge status have changed, since adjusters
        // and toEdge observers may need to react to changes in either or both.
        if (changedPageOptedIn || changedDrawToEdge) {
            boolean shouldPad = shouldPadAdjusters();
            for (var adjuster : mPadAdjusters) {
                adjuster.overrideBottomInset(
                        shouldPad ? mSystemInsets.bottom : 0,
                        shouldPad && !mBottomControlsAreVisible ? mSystemInsets.bottom : 0);
            }
            for (var observer : mEdgeChangeObservers) {
                observer.onToEdgeChange(isPageOptedIntoEdgeToEdge() ? mSystemInsets.bottom : 0);
            }
        }
    }

    @NonNull
    private WindowInsetsCompat handleWindowInsets(
            View rootView, @NonNull WindowInsetsCompat windowInsets) {
        Insets newInsets = getSystemInsets(windowInsets);
        Insets newKeyboardInsets = windowInsets.getInsets(WindowInsetsCompat.Type.ime());

        if (!newInsets.equals(mSystemInsets) || !newKeyboardInsets.equals(mKeyboardInsets)) {
            mSystemInsets = newInsets;
            mKeyboardInsets = newKeyboardInsets;

            // When a foldable goes to/from tablet mode we must reassess.
            // TODO(https://crbug.com/325356134) Find a cleaner check and remedy.
            mIsPageOptedIntoEdgeToEdge =
                    mIsPageOptedIntoEdgeToEdge
                            && EdgeToEdgeControllerFactory.isSupportedConfiguration(mActivity);
            // Note that we cannot #drawToEdge earlier since we need the system
            // insets.
            drawToEdge(mIsPageOptedIntoEdgeToEdge);
        }
        return windowInsets;
    }

    /**
     * The {@link EdgeToEdgePadAdjuster}s should only be padded with an extra bottom inset if the
     * activity is currently in edge-to-edge, and if the adjusters aren't already positioned above
     * the system insets due to the keyboard or the bottom controls being visible.
     */
    // TODO(crbug.com/350544729) Update to account for the bottom chin (particularly when scrolled
    //  off).
    private boolean shouldPadAdjusters() {
        boolean keyboardIsVisible = mKeyboardInsets != null && mKeyboardInsets.bottom > 0;
        return mIsPageOptedIntoEdgeToEdge && !keyboardIsVisible;
    }

    /**
     * Adjusts whether the given view draws ToEdge or ToNormal. The ability to draw under System
     * Bars should have already been set. This method only sets the padding of the view and
     * transparency of the Nav Bar, etc.
     *
     * @param toEdge Whether to adjust the drawing environment ToEdge.
     * @param contentView The content view in the window.
     */
    private void adjustEdges(boolean toEdge, View contentView) {
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
                contentView,
                mSystemInsets.left,
                mSystemInsets.top,
                mSystemInsets.right,
                bottomPadding);

        int bottomInsetOnSaveArea = toEdge ? mSystemInsets.bottom : 0;
        mInsetObserver.updateBottomInsetForEdgeToEdge(bottomInsetOnSaveArea);
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

    private static Insets getSystemInsets(@NonNull WindowInsetsCompat windowInsets) {
        return windowInsets.getInsets(
                WindowInsetsCompat.Type.navigationBars() + WindowInsetsCompat.Type.statusBars());
    }
}
