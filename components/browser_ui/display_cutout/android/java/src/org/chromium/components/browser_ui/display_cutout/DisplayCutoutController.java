// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.display_cutout;

import static androidx.core.view.WindowInsetsCompat.Type.navigationBars;
import static androidx.core.view.WindowInsetsCompat.Type.statusBars;
import static androidx.core.view.WindowInsetsCompat.Type.systemBars;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.view.Display;
import android.view.View;
import android.view.Window;
import android.view.WindowManager.LayoutParams;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.DisplayCutoutCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;

/**
 * Controls the display safe area for a {@link WebContents} and the cutout mode for an {@link
 * Activity} window.
 *
 * <p>The WebContents is updated with the safe area continuously, as long as {@link
 * Delegate#getAttachedActivity()} returns a non-null value. The cutout mode is set on the
 * Activity's window only in P+, and when either the associated WebContents is fullscreen or the
 * embedder reports browser fullscreen mode.
 */
@NullMarked
public class DisplayCutoutController implements InsetObserver.WindowInsetObserver, UserData {
    private static final String TAG = "E2E_DCController";

    private static final Class<DisplayCutoutController> USER_DATA_KEY =
            DisplayCutoutController.class;
    private static final int INVALID_DISPLAY_ROTATION = -1;
    private static final Rect EMPTY_RECT = new Rect();

    /** {@link Window} of the current {@link Activity}. */
    private @Nullable Window mWindow;

    /** The current viewport fit value. */
    private @WebContentsObserver.ViewportFitType int mViewportFit = ViewportFit.AUTO;

    /**
     * The current {@link InsetObserver} that we are attached to. This can be null if we have not
     * attached to an activity.
     */
    private @Nullable InsetObserver mInsetObserver;

    /**
     * Provides the activity-specific (vs tab-specific) cutout mode. The activity-specific cutout
     * mode takes precedence over the tab-specific cutout mode.
     */
    private @Nullable MonotonicObservableSupplier<Integer> mBrowserCutoutModeSupplier;

    /** Observes {@link mBrowserCutoutModeSupplier}. */
    private @Nullable Callback<Integer> mBrowserCutoutModeObserver;

    /** Observes {@link Delegate#getWebContents()}. */
    private @Nullable FullscreenWebContentsObserver mWebContentsObserver;

    /** Tracks Safe Area Insets. */
    private final SafeAreaInsetsTrackerImpl mSafeAreaInsetsTracker;

    private Rect mCachedSafeAreaInsets = new Rect();
    private Rect mCachedBrowserSafeAreaInsets = new Rect();
    private int mCachedBrowserSafeAreaInsetsRotation = INVALID_DISPLAY_ROTATION;

    /**
     * An interface to track general changes to Safe Area Insets. TODO(crbug.com/40279791) Develop
     * beyond this minimal stub.
     */
    public interface SafeAreaInsetsTracker {

        /**
         * @return whether this Tracker was created for a web page set to Cover.
         */
        boolean isViewportFitCover();

        /** Return whether the safe area is constrained on the current web page. */
        boolean hasSafeAreaConstraint();
    }

    /**
     * Tracks general changes to Safe Area Insets. TODO(crbug.com/40279791) Track the Notch and
     * bottom in a class in a separate file.
     */
    private static class SafeAreaInsetsTrackerImpl implements SafeAreaInsetsTracker {
        private boolean mIsViewportFitCover;
        private boolean mHasSafeAreaConstraint;

        /** Sets whether this Tracker was created for a web page set to Cover. */
        public void setIsViewportFitCover(boolean isViewportFitCover) {
            mIsViewportFitCover = isViewportFitCover;
        }

        @Override
        public boolean isViewportFitCover() {
            return mIsViewportFitCover;
        }

        /** Sets whether there are safe area constraint for the web page. */
        public void setSafeAreaConstraint(boolean hasConstraint) {
            mHasSafeAreaConstraint = hasConstraint;
        }

        @Override
        public boolean hasSafeAreaConstraint() {
            return mHasSafeAreaConstraint;
        }
    }

    /** An interface for providing embedder-specific behavior to the controller. */
    public interface Delegate {

        /** Returns the activity this controller is associated with, if there is one. */
        @Nullable Activity getAttachedActivity();

        /**
         * Returns the {@link WebContents} this controller should update the safe area for, if there
         * is one.
         */
        @Nullable WebContents getWebContents();

        /**
         * Returns the InsetObserver this controller uses for safe area updates, if there is one.
         */
        @Nullable InsetObserver getInsetObserver();

        /** Returns whether the user can interact with the associated WebContents/UI element. */
        boolean isInteractable();

        /**
         * Returns the activity-specific (vs tab-specific) cutout mode. The activity-specific cutout
         * mode takes precedence over the tab-specific cutout mode.
         */
        @Nullable MonotonicObservableSupplier<Integer> getBrowserDisplayCutoutModeSupplier();

        /** Returns the resolved display mode for the embedder. */
        @DisplayMode.EnumType
        int getDisplayMode();

        /** Whether the basic Feature for drawing Edge To Edge is enabled. */
        boolean isDrawEdgeToEdgeEnabled();

        /**
         * Requests or releases edge-to-edge drawing on the embedder's window. Called by the
         * controller on every viewport-fit update, so implementations may receive repeated calls
         * with the same value; they should also use this hook to re-sync related surfaces such as
         * system bar coloring.
         *
         * @param drawEdgeToEdge whether the window should currently draw edge-to-edge.
         */
        void setEdgeToEdgeState(boolean drawEdgeToEdge);

        /**
         * Whether the embedder's short-edges cutout mode is enabled. When false, the controller
         * falls back to its pre-flag behavior so it can serve as a killswitch for the new
         * cover-mode logic (browser safe-area sourcing and cached fallback).
         */
        default boolean isShortEdgesCutoutModeEnabled() {
            return false;
        }
    }

    // Helper implementation to observe fullscreen changes and trigger re-layout.
    private class FullscreenWebContentsObserver extends WebContentsObserver {
        FullscreenWebContentsObserver(WebContents webContents) {
            super(webContents);
        }

        @Override
        public void didToggleFullscreenModeForTab(
                boolean enteredFullscreen, boolean willCauseResize) {
            maybeUpdateLayout();
        }
    }

    private final Delegate mDelegate;

    /**
     * Gets the DisplayCutoutController from the current {@link Tab} if there is one, otherwise
     * {@code null} is returned.
     */
    private static @Nullable DisplayCutoutController from(Tab tab) {
        if (tab.isDestroyed()) return null;
        UserDataHost host = tab.getUserDataHost();
        return host.getUserData(USER_DATA_KEY);
    }

    /**
     * Gets the DisplayCutoutController from the current {@link Tab}, creating one if needed.
     *
     * @param tab The Tab to get the controller from.
     * @param delegate A delegate to embedder-specific behavior to the controller.
     */
    public static DisplayCutoutController createForTab(Tab tab, Delegate delegate) {
        UserDataHost host = tab.getUserDataHost();
        DisplayCutoutController controller = host.getUserData(USER_DATA_KEY);
        return controller != null
                ? controller
                : host.setUserData(USER_DATA_KEY, new DisplayCutoutController(delegate));
    }

    /**
     * Constructs a controller for the Notch in the Display. Works with a {@code
     * DisplayCutoutTabHelper} to track changes to the Viewport for the current Tab and allow
     * drawing around the Notch and pushing Safe Area Insets back to Blink for the web page.
     *
     * @param delegate Provides access to the environment in which this runs, e.g. the Activity.
     *     TODO(crbug.com/40279791) make this constructor package-private when refactoring.
     */
    @VisibleForTesting
    public DisplayCutoutController(Delegate delegate) {
        mDelegate = delegate;
        mSafeAreaInsetsTracker = new SafeAreaInsetsTrackerImpl();
        maybeAddObservers();
    }

    /**
     * Add observers to {@link InsetObserver} and the browser display cutout mode supplier if we
     * have not added them.
     */
    @VisibleForTesting
    public void maybeAddObservers() {
        Activity activity = mDelegate.getAttachedActivity();
        if (activity == null) return;

        updateInsetObserver(mDelegate.getInsetObserver());
        updateBrowserCutoutObserver(mDelegate.getBrowserDisplayCutoutModeSupplier());
        updateWebContentObserver(mDelegate.getWebContents());
        mWindow = activity.getWindow();
        updateEdgeToEdgeMode(shouldUseBrowserEdgeToEdge());
    }

    /** Remove observers added by {@link #maybeAddObservers()}. */
    void removeObservers() {
        updateEdgeToEdgeMode(/* useEdgeToEdge= */ false);
        updateInsetObserver(null);
        updateBrowserCutoutObserver(null);
        if (mWebContentsObserver != null) {
            mWebContentsObserver.observe(null);
            mWebContentsObserver = null;
        }
        mWindow = null;
    }

    private void updateInsetObserver(@Nullable InsetObserver observer) {
        if (mInsetObserver == observer) return;

        if (mInsetObserver != null) {
            mInsetObserver.removeObserver(this);
        }
        mInsetObserver = observer;
        if (mInsetObserver != null) {
            mInsetObserver.addObserver(this);

            // For E2E pages, populate the SAI during initialization.
            if (mDelegate.isDrawEdgeToEdgeEnabled()) {
                maybePushSafeAreaInsets(mInsetObserver.getCurrentSafeArea());
            }
        }
    }

    private void updateBrowserCutoutObserver(
            @Nullable MonotonicObservableSupplier<Integer> supplier) {
        if (mBrowserCutoutModeSupplier == supplier) return;

        if (mBrowserCutoutModeObserver != null && mBrowserCutoutModeSupplier != null) {
            mBrowserCutoutModeSupplier.removeObserver(mBrowserCutoutModeObserver);
        }
        mBrowserCutoutModeSupplier = supplier;
        mBrowserCutoutModeObserver = null;
        if (mBrowserCutoutModeSupplier != null) {
            mBrowserCutoutModeObserver =
                    (browserDisplayCutoutMode) -> {
                        maybeUpdateLayout();
                    };
            mBrowserCutoutModeSupplier.addSyncObserverAndPostIfNonNull(mBrowserCutoutModeObserver);
        }
    }

    private void updateWebContentObserver(@Nullable WebContents webContents) {
        if (mWebContentsObserver != null
                && webContents != null
                && mWebContentsObserver.getWebContents() == webContents) {
            return;
        }

        if (mWebContentsObserver != null) {
            mWebContentsObserver.observe(null);
            mWebContentsObserver = null;
        }

        if (webContents == null) return;

        mWebContentsObserver = new FullscreenWebContentsObserver(webContents);
    }

    @Override
    public void destroy() {
        removeObservers();
    }

    /**
     * Set the viewport fit value for the tab.
     *
     * @param value The new viewport fit value.
     */
    public void setViewportFit(@WebContentsObserver.ViewportFitType int value) {
        Log.i(TAG, "setViewportFit: %s", value);
        mSafeAreaInsetsTracker.setIsViewportFitCover(shouldTreatViewportFitAsCover(value));

        // TODO(crbug.com/40281421): Investigate whether if() can be turned into assert.
        // Most likely we will need to just remove this section when E2E is launched.
        if (!mDelegate.isDrawEdgeToEdgeEnabled()
                && !assumeNonNull(mDelegate.getWebContents()).isFullscreenForCurrentTab()
                && !isInEdgeToEdgeCompatibleDisplayMode()) {
            value = ViewportFit.AUTO;
        }

        boolean viewportFitChanged = value != mViewportFit;
        mViewportFit = value;
        // Re-process unchanged cover-mode reports too (e.g. a page reloading still at
        // viewport-fit=cover) so Blink gets the current insets again.
        if (!viewportFitChanged && !shouldTreatViewportFitAsCover(value)) return;

        maybeUpdateLayout();
        if (mDelegate.isDrawEdgeToEdgeEnabled()) {
            // Re-push the safe area when a cover-mode page reports viewport-fit=cover again after
            // reload. Blink needs the current insets for CSS env() variables even if the enum value
            // did not change.
            maybePushSafeAreaInsets(mCachedSafeAreaInsets);
        }
    }

    /**
     * Set whether there are safe area constraint on the current web page.
     *
     * @param hasConstraint whether the safe area is constrained on the current web page.
     */
    public void setSafeAreaConstraint(boolean hasConstraint) {
        Log.i(TAG, "setSafeAreaConstraint: %b", hasConstraint);
        mSafeAreaInsetsTracker.setSafeAreaConstraint(hasConstraint);
    }

    /**
     * Gets the {@link SafeAreaInsetsTracker} associated with the given Tab.
     *
     * @param tab The {@link Tab} that may have a web page rendered already
     * @return A {@link SafeAreaInsetsTracker} to track changes in insets for viewport-fit=cover.
     */
    public static @Nullable SafeAreaInsetsTracker getSafeAreaInsetsTracker(Tab tab) {
        DisplayCutoutController displayCutoutControllerInstanceForTab = from(tab);
        if (displayCutoutControllerInstanceForTab == null) return null;
        return displayCutoutControllerInstanceForTab.mSafeAreaInsetsTracker;
    }

    /** Implements {@link WindowInsetsObserver}. */
    @Override
    public void onSafeAreaChanged(Rect area) {
        maybePushSafeAreaInsets(area);
    }

    @Override
    public void onInsetChanged() {
        // In app-style browser fullscreen (for example display: standalone), the safe area from
        // InsetObserver can be 0 because the cutout is fully contained by the status bar. Re-push
        // insets when system bars change so we can merge in the raw system-bar values.
        if (!shouldMergeWithBrowserSafeAreaInsets()) return;

        maybePushSafeAreaInsets(mCachedSafeAreaInsets);
    }

    private void maybePushSafeAreaInsets(Rect area) {
        WebContents webContents = mDelegate.getWebContents();
        if (webContents == null) return;
        if (webContents.getTopLevelNativeWindow() == null) return;

        mCachedSafeAreaInsets = area;
        Rect mergedSafeAreaInsets = maybeMergeWithBrowserSafeAreaInsets(area);
        float dipScale = getDipScale();
        Rect safeArea =
                new Rect(
                        adjustInsetForScale(mergedSafeAreaInsets.left, dipScale),
                        adjustInsetForScale(mergedSafeAreaInsets.top, dipScale),
                        adjustInsetForScale(mergedSafeAreaInsets.right, dipScale),
                        adjustInsetForScale(mergedSafeAreaInsets.bottom, dipScale));

        // Notify Blink of the new insets for css env() variables.
        webContents.setDisplayCutoutSafeArea(safeArea);
    }

    /**
     * In app-style browser fullscreen modes (e.g. webapps with display:standalone or app-style CCT)
     * the per-WebContents safe area reported by Blink does not include the browser's system bar
     * geometry, but CSS env(safe-area-inset-*) authors expect a single rect that covers everything
     * obstructing the viewport. This helper merges the WebContents safe area with the browser's own
     * bar/cutout insets so the page sees a complete safe area.
     *
     * <p>When the embedder's short-edges cutout mode is enabled, the browser safe area is also
     * cached per display rotation. Reload / pull-to-refresh paths can transiently report all-zero
     * raw insets even though the bar geometry has not changed; the cache lets the page keep its CSS
     * env(safe-area-inset-*) values across that hiccup. The cache is invalidated across rotations
     * because portrait and landscape can have different browser safe areas.
     *
     * @param safeAreaInsets the per-WebContents safe area reported by Blink.
     * @return the merged safe area to push back to Blink, or {@code safeAreaInsets} unchanged when
     *     no merge is required (e.g. fullscreen HTML mode, or no browser insets to merge).
     */
    private Rect maybeMergeWithBrowserSafeAreaInsets(Rect safeAreaInsets) {
        if (!shouldMergeWithBrowserSafeAreaInsets()) return safeAreaInsets;

        Rect browserSafeAreaInsets = getBrowserSafeAreaInsets();
        if (mDelegate.isShortEdgesCutoutModeEnabled()) {
            // Reload paths can transiently report all-zero raw insets even though the bar
            // geometry has not changed. Fall back to the last non-zero browser safe area so the
            // page does not lose its CSS env(safe-area-inset-*) values on refresh. Do not reuse the
            // cache across display rotations, since portrait and landscape can have different
            // browser safe areas.
            int currentRotation = getDisplayRotationForWindow(mWindow);
            if (EMPTY_RECT.equals(browserSafeAreaInsets)) {
                if (currentRotation == INVALID_DISPLAY_ROTATION
                        || currentRotation == mCachedBrowserSafeAreaInsetsRotation) {
                    browserSafeAreaInsets = mCachedBrowserSafeAreaInsets;
                }
            } else {
                mCachedBrowserSafeAreaInsets = browserSafeAreaInsets;
                mCachedBrowserSafeAreaInsetsRotation = currentRotation;
            }
        }

        if (EMPTY_RECT.equals(browserSafeAreaInsets)) {
            return safeAreaInsets;
        }

        return getMaxRect(safeAreaInsets, browserSafeAreaInsets);
    }

    /**
     * Returns the area on the activity window that is currently obstructed by browser chrome
     * (status bar, navigation bar, and any display cutout). Together these represent everything a
     * web page should treat as "not safe to draw under" once the controller is in browser
     * edge-to-edge mode, and they are merged with the per-WebContents safe area before being pushed
     * back to Blink for CSS env(safe-area-inset-*).
     *
     * <p>In short-edges cutout mode we read the bar geometry via {@link
     * WindowInsetsCompat#getInsetsIgnoringVisibility} so transient bar visibility changes (e.g.
     * while the IME is showing) don't collapse the safe area; otherwise we use the visible
     * system-bar insets to preserve the pre-flag behavior.
     *
     * @return the union of browser bar and cutout insets, or an empty Rect if no insets are
     *     available yet.
     */
    private Rect getBrowserSafeAreaInsets() {
        if (mInsetObserver == null) return new Rect();

        WindowInsetsCompat windowInsets = mInsetObserver.getLastRawWindowInsets();
        if (windowInsets == null) return new Rect();

        Rect barInsets;
        if (mDelegate.isShortEdgesCutoutModeEnabled()) {
            // Use stable bar geometry so reload / pull-to-refresh does not transiently zero
            // CSS env(safe-area-inset-*) when bars are momentarily reported as not visible.
            Insets statusBarInsets = windowInsets.getInsetsIgnoringVisibility(statusBars());
            Insets navigationBarInsets = windowInsets.getInsetsIgnoringVisibility(navigationBars());
            barInsets = getMaxRect(toRect(statusBarInsets), toRect(navigationBarInsets));
        } else {
            barInsets = toRect(windowInsets.getInsets(systemBars()));
        }

        DisplayCutoutCompat displayCutout = windowInsets.getDisplayCutout();
        if (displayCutout == null) return barInsets;

        Rect cutoutInsets =
                new Rect(
                        displayCutout.getSafeInsetLeft(),
                        displayCutout.getSafeInsetTop(),
                        displayCutout.getSafeInsetRight(),
                        displayCutout.getSafeInsetBottom());
        return getMaxRect(barInsets, cutoutInsets);
    }

    private static Rect getMaxRect(Rect a, Rect b) {
        return new Rect(
                Math.max(a.left, b.left),
                Math.max(a.top, b.top),
                Math.max(a.right, b.right),
                Math.max(a.bottom, b.bottom));
    }

    private static Rect toRect(Insets insets) {
        return new Rect(insets.left, insets.top, insets.right, insets.bottom);
    }

    /**
     * Adjusts a WindowInset inset to a CSS pixel value.
     *
     * @param inset The inset as an integer.
     * @param dipScale The devices dip scale as an integer.
     * @return The CSS pixel value adjusted for scale.
     */
    private static int adjustInsetForScale(int inset, float dipScale) {
        return (int) Math.ceil(inset / dipScale);
    }

    private static int getDisplayRotationForWindow(@Nullable Window window) {
        if (window == null) return INVALID_DISPLAY_ROTATION;

        View decorView = window.getDecorView();
        if (decorView == null) return INVALID_DISPLAY_ROTATION;

        Display display = decorView.getDisplay();
        return display == null ? INVALID_DISPLAY_ROTATION : display.getRotation();
    }

    private boolean shouldMergeWithBrowserSafeAreaInsets() {
        return shouldUseBrowserEdgeToEdge();
    }

    private boolean shouldUseBrowserEdgeToEdge() {
        return mDelegate.isShortEdgesCutoutModeEnabled()
                && mDelegate.isDrawEdgeToEdgeEnabled()
                && isInEdgeToEdgeCompatibleDisplayMode()
                && shouldTreatViewportFitAsCover(mViewportFit);
    }

    private boolean isInEdgeToEdgeCompatibleDisplayMode() {
        @DisplayMode.EnumType int displayMode = mDelegate.getDisplayMode();
        if (!mDelegate.isShortEdgesCutoutModeEnabled()) {
            return displayMode == DisplayMode.FULLSCREEN;
        }
        return displayMode == DisplayMode.FULLSCREEN || displayMode == DisplayMode.STANDALONE;
    }

    private static boolean shouldTreatViewportFitAsCover(
            @WebContentsObserver.ViewportFitType int value) {
        return value == ViewportFit.COVER || value == ViewportFit.COVER_FORCED_BY_USER_AGENT;
    }

    private void updateEdgeToEdgeMode(boolean useEdgeToEdge) {
        // The delegate owns the edge-to-edge token; setEdgeToEdgeState is idempotent and safe to
        // call on every update.
        mDelegate.setEdgeToEdgeState(useEdgeToEdge);
    }

    @VisibleForTesting
    protected float getDipScale() {
        return assumeNonNull(assumeNonNull(mDelegate.getWebContents()).getTopLevelNativeWindow())
                .getDisplay()
                .getDipScale();
    }

    /**
     * Converts a {@link ViewportFit} value into the Android P+ equivalent.
     *
     * @return String containing the {@link LayoutParams} field name of the equivalent value.
     */
    @VisibleForTesting
    @RequiresApi(Build.VERSION_CODES.P)
    public int computeDisplayCutoutMode() {
        // If we are not interactable then force the default mode.
        if (!mDelegate.isInteractable()) {
            return LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        }

        if (mBrowserCutoutModeSupplier != null) {
            Integer mode = mBrowserCutoutModeSupplier.get();
            if (mode != null) {
                int browserCutoutMode = mode;
                if (browserCutoutMode != LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT) {
                    return browserCutoutMode;
                }
            }
        }

        // Never draw under notch unless either the page is in fullscreen mode or the embedder
        // reports browser fullscreen mode (for example, app-style webapp windows).
        WebContents webContents = mDelegate.getWebContents();
        boolean isHtmlFullscreen = webContents != null && webContents.isFullscreenForCurrentTab();
        if (!isHtmlFullscreen && !isInEdgeToEdgeCompatibleDisplayMode()) {
            return LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        }

        if (mViewportFit == ViewportFit.CONTAIN) {
            return LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
        }
        if (shouldTreatViewportFitAsCover(mViewportFit)) {
            return LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
        return LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
    }

    @VisibleForTesting
    protected @Nullable LayoutParams getWindowAttributes() {
        return mWindow == null ? null : mWindow.getAttributes();
    }

    @VisibleForTesting
    protected void setWindowAttributes(LayoutParams attributes) {
        assumeNonNull(mWindow);
        mWindow.setAttributes(attributes);
    }

    /** Should be called to refresh the activity window's layout based on current state. */
    public void maybeUpdateLayout() {
        updateEdgeToEdgeMode(shouldUseBrowserEdgeToEdge());
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        LayoutParams attributes = getWindowAttributes();
        if (attributes == null) return;

        final int displayCutoutMode = computeDisplayCutoutMode();
        if (attributes.layoutInDisplayCutoutMode == displayCutoutMode) return;

        attributes.layoutInDisplayCutoutMode = displayCutoutMode;
        setWindowAttributes(attributes);
    }

    /** Should be called when the associated UI surface is attached or detached to an activity. */
    public void onActivityAttachmentChanged(@Nullable WindowAndroid window) {
        if (window == null) {
            removeObservers();
        } else {
            maybeAddObservers();
        }
    }

    /** Called when web contents changed in the attached tab. */
    public void onContentChanged() {
        updateWebContentObserver(mDelegate.getWebContents());
        // The new WebContents starts with cleared display-cutout safe area state. If the page is
        // already known to be in cover mode (e.g. same-page reload), proactively re-push the
        // cached insets so CSS env(safe-area-inset-*) does not collapse to 0 across reload.
        if (shouldUseBrowserEdgeToEdge()) {
            maybePushSafeAreaInsets(mCachedSafeAreaInsets);
        }
    }

    /**
     * Called by the embedder once the page load has finished and the renderer is ready to apply a
     * new display-cutout safe area for env(safe-area-inset-*).
     */
    public void onPageLoadFinished() {
        if (shouldUseBrowserEdgeToEdge()) {
            maybePushSafeAreaInsets(mCachedSafeAreaInsets);
        }
    }

    public @Nullable WebContentsObserver getWebContentObserverForTesting() {
        return mWebContentsObserver;
    }
}
