// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.Window;
import android.view.WindowInsets;

import androidx.annotation.IntDef;
import androidx.annotation.OptIn;
import androidx.core.graphics.Insets;
import androidx.core.os.BuildCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.ApkInfo;
import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController.SafeAreaInsetsTracker;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.insets.InsetObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A util helper class to know if e2e is on and eligible for current session and to record metrics
 * when necessary.
 */
@NullMarked
public class EdgeToEdgeUtils {
    private static final String TAG = "E2E_Utils";
    private static @Nullable Boolean sIsTargetSdkEnforceEdgeToEdge;
    private static boolean sObservedTappableNavigationBar;
    private static boolean sAlwaysDrawWebEdgeToEdgeForTesting;
    private static @Nullable Boolean sHas3ButtonNavBarForTesting;

    private static final String ELIGIBLE_HISTOGRAM = "Android.EdgeToEdge.Eligible";
    private static final String INELIGIBLE_REASON_HISTOGRAM =
            "Android.EdgeToEdge.IneligibilityReason";
    private static final String PARAM_SAFE_AREA_CONSTRAINT_SCROLLABLE_WHEN_STACKING =
            "scrollable_when_stacking";
    private static final String MISSING_NAVBAR_INSETS_HISTOGRAM =
            "Android.EdgeToEdge.MissingNavbarInsets2";

    /** The reason of why the current session is not eligible for edge to edge. */
    @IntDef({
        IneligibilityReason.OS_VERSION,
        IneligibilityReason.FORM_FACTOR,
        IneligibilityReason.NAVIGATION_MODE,
        IneligibilityReason.DEVICE_TYPE,
        IneligibilityReason.NUM_TYPES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface IneligibilityReason {
        int OS_VERSION = 0;
        int FORM_FACTOR = 1;
        int NAVIGATION_MODE = 2;
        int DEVICE_TYPE = 3;
        int NUM_TYPES = 4;
    }

    /** The reason of why the navigation bar insets are missing. */
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        MissingNavbarInsetsReason.OTHER,
        MissingNavbarInsetsReason.IN_MULTI_WINDOW,
        MissingNavbarInsetsReason.IN_DESKTOP_WINDOW,
        MissingNavbarInsetsReason.IN_FULLSCREEN,
        MissingNavbarInsetsReason.ACTIVITY_NOT_VISIBLE,
        MissingNavbarInsetsReason.SYSTEM_BAR_INSETS_EMPTY,
        MissingNavbarInsetsReason.NUM_ENTRIES
    })
    public @interface MissingNavbarInsetsReason {
        int OTHER = 0;
        int IN_MULTI_WINDOW = 1;
        int IN_DESKTOP_WINDOW = 2;
        int IN_FULLSCREEN = 3;
        int ACTIVITY_NOT_VISIBLE = 4;
        int SYSTEM_BAR_INSETS_EMPTY = 5;

        int NUM_ENTRIES = 5;
    }

    /**
     * Whether the draw edge to edge infrastructure is on. When this is enabled, Chrome will start
     * drawing edge to edge on start up.
     *
     * <p>To check if Chrome is aware of edge-to-edge, use {@link
     * org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeStateProvider#get()}
     *
     * @deprecated Avoid new usage. Check corresponding feature flags inline.
     */
    @Deprecated
    public static boolean isChromeEdgeToEdgeFeatureEnabled() {
        return isBottomChinFeatureEnabled() || isEdgeToEdgeEverywhereEnabled();
    }

    /**
     * Whether the edge-to-edge bottom chin is enabled.
     *
     * <p>When enabled, Chrome will replace the OS navigation bar with a thin "Chin" layer in the
     * browser controls and can be scrolled off the screen on web pages.
     */
    public static boolean isBottomChinFeatureEnabled() {
        return ChromeFeatureList.sEdgeToEdgeBottomChin.isEnabled();
    }

    /**
     * Returns whether the configuration of the device should allow Edge To Edge bottom chin. Note
     * the results are false-positive, if the method is called before the |activity|'s decor view
     * being attached to the window.
     */
    public static boolean isEdgeToEdgeBottomChinEnabled(Activity activity) {
        // Make sure we test SDK version before checking the Feature so Field Trials only collect
        // from qualifying devices.
        if (!EdgeToEdgeFieldTrial.getBottomChinOverrides().isEnabledForManufacturerVersion()) {
            return false;
        }

        // The root view's window insets is too soon to determine if we are in 3-button gesture nav
        // mode.
        if (activity == null
                || activity.getWindow() == null
                || activity.getWindow().getDecorView().getRootWindowInsets() == null) {
            return false;
        }

        // Not supported on tablet unless the flag is on and it meets the minimum screen size.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)
                && (!isEdgeToEdgeTabletEnabled() || !EdgeToEdgeUtils.isSupportedTablet(activity))) {
            return false;
        }

        return isBottomChinFeatureEnabled()
                && !BuildInfo.getInstance().isAutomotive
                && !hasTappableNavigationBar(activity.getWindow());
    }

    /**
     * This is a sensitive check for whether all insets indicate or imply that the device is in
     * gesture navigation mode, and not tappable (3-button) navigation mode.
     *
     * @param insetObserver The InsetObserver in the current activity, used to retrieve the last
     *     seen root view window insets.
     * @return Whether all insets indicate the device is in gesture navigation mode.
     */
    public static boolean doAllInsetsIndicateGestureNavigation(InsetObserver insetObserver) {
        WindowInsetsCompat rootInsets = insetObserver.getLastRawWindowInsets();
        return rootInsets != null
                && isInGestureNavigationMode(rootInsets)
                && !hasTappableBarIgnoringTop(() -> rootInsets);
    }

    /** Whether the edge-to-edge feature is enabled on tablet. */
    public static boolean isEdgeToEdgeTabletEnabled() {
        return ChromeFeatureList.sEdgeToEdgeTablet.isEnabled();
    }

    /**
     * Whether the device is a tablet and supports edge-to-edge.
     *
     * <ul>
     *   <li>width < MinWidthThreshold: e2e disabled.
     *   <li>MinWidthThreshold <= width < InvisibleBottomChinMinWidth: e2e enabled and the bottom
     *       chin is visible by default. Same as behavior on phone.
     *   <li>InvisibleBottomChinMinWidth <= width: fully e2e and the bottom chin is invisible by
     *       default.
     * </ul>
     */
    public static boolean isSupportedTablet(Context context) {
        int widthThreshold = ChromeFeatureList.sEdgeToEdgeTabletMinWidthThreshold.getValue();
        if (widthThreshold == -1) {
            return true;
        }
        return DisplayUtil.getCurrentSmallestScreenWidth(context) >= widthThreshold;
    }

    /** Whether the device is a tablet and supports edge-to-edge. */
    public static boolean defaultVisibilityOfBottomChinOnTablet(Context context) {
        int widthThreshold =
                ChromeFeatureList.sEdgeToEdgeTabletInvisibleBottomChinMinWidth.getValue();
        if (widthThreshold == -1) {
            return false;
        }
        return DisplayUtil.getCurrentSmallestScreenWidth(context) < widthThreshold;
    }

    /**
     * Whether drawing the website that has `viewport-fit=cover` fully edge to edge, removing the
     * bottom chin.
     */
    public static boolean isEdgeToEdgeWebOptInEnabled() {
        return isBottomChinFeatureEnabled() && ChromeFeatureList.sEdgeToEdgeWebOptIn.isEnabled();
    }

    /** Whether edge-to-edge should be enabled everywhere. */
    @OptIn(markerClass = BuildCompat.PrereleaseSdkCheck.class)
    public static boolean isEdgeToEdgeEverywhereEnabled() {
        if (!EdgeToEdgeFieldTrial.getEverywhereOverrides().isEnabledForManufacturerVersion()) {
            return false;
        }

        if (BuildInfo.getInstance().isAutomotive || BuildInfo.getInstance().isDesktop) {
            return false;
        }

        if (ChromeFeatureList.sEdgeToEdgeEverywhere.isEnabled()) {
            return true;
        }

        if (sIsTargetSdkEnforceEdgeToEdge == null) {
            // TODO(crbug.com/394945134): Switch to SDK_INT / BuildCompat when it's available.
            sIsTargetSdkEnforceEdgeToEdge = ApkInfo.targetAtLeastB() && BuildCompat.isAtLeastB();
            Log.i(TAG, "sIsTargetSdkEnforceEdgeToEdge " + sIsTargetSdkEnforceEdgeToEdge);
        }
        return sIsTargetSdkEnforceEdgeToEdge;
    }

    /** Whether turn on the debug paint for edge to edge layout. */
    public static boolean isEdgeToEdgeEverywhereDebugging() {
        return ChromeFeatureList.sEdgeToEdgeEverywhereIsDebugging.getValue();
    }

    /** Whether key native pages should draw to edge. */
    public static boolean isDrawKeyNativePageToEdgeEnabled() {
        return isBottomChinFeatureEnabled()
                && ChromeFeatureList.sDrawKeyNativeEdgeToEdge.isEnabled();
    }

    /**
     * Whether reporting the page's safe area constraint to the bottom chin. Required when {@link
     * isEdgeToEdgeBottomChinEnabled}.
     */
    public static boolean isSafeAreaConstraintEnabled() {
        return isBottomChinFeatureEnabled()
                && ChromeFeatureList.sEdgeToEdgeSafeAreaConstraint.isEnabled();
    }

    /** Whether the bottom chin should ignore the constraint when stacking with other layers. */
    public static boolean isConstraintBottomChinScrollableWhenStacking() {
        return isSafeAreaConstraintEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.EDGE_TO_EDGE_SAFE_AREA_CONSTRAINT,
                        PARAM_SAFE_AREA_CONSTRAINT_SCROLLABLE_WHEN_STACKING,
                        true);
    }

    /**
     * Record if the current activity is eligible for edge to edge. If not, also record the reason
     * why it is ineligible.
     *
     * @param activity The current active activity.
     * @return Whether the activity is eligible for edge to edge based on device configuration.
     */
    public static boolean recordEligibility(Activity activity) {
        boolean eligible = true;

        if (hasTappableNavigationBar(activity.getWindow())) {
            eligible = false;
            RecordHistogram.recordEnumeratedHistogram(
                    INELIGIBLE_REASON_HISTOGRAM,
                    IneligibilityReason.NAVIGATION_MODE,
                    IneligibilityReason.NUM_TYPES);
        }

        // Not supported on tablet unless the flag is on and it meets the minimum screen size.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)
                && (!isEdgeToEdgeTabletEnabled() || !EdgeToEdgeUtils.isSupportedTablet(activity))) {
            eligible = false;
            RecordHistogram.recordEnumeratedHistogram(
                    INELIGIBLE_REASON_HISTOGRAM,
                    IneligibilityReason.FORM_FACTOR,
                    IneligibilityReason.NUM_TYPES);
        }

        if (android.os.Build.VERSION.SDK_INT < VERSION_CODES.R) {
            eligible = false;
            RecordHistogram.recordEnumeratedHistogram(
                    INELIGIBLE_REASON_HISTOGRAM,
                    IneligibilityReason.OS_VERSION,
                    IneligibilityReason.NUM_TYPES);
        }

        if (BuildInfo.getInstance().isAutomotive) {
            eligible = false;
            RecordHistogram.recordEnumeratedHistogram(
                    INELIGIBLE_REASON_HISTOGRAM,
                    IneligibilityReason.DEVICE_TYPE,
                    IneligibilityReason.NUM_TYPES);
        }
        RecordHistogram.recordBooleanHistogram(ELIGIBLE_HISTOGRAM, eligible);

        return eligible;
    }

    /**
     * Record if the current activity is missing the navigation bar.
     *
     * @param reason The reason of why the navigation bar is missing.
     */
    public static void recordIfMissingNavigationBar(@MissingNavbarInsetsReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                MISSING_NAVBAR_INSETS_HISTOGRAM, reason, MissingNavbarInsetsReason.NUM_ENTRIES);
    }

    /**
     * @param isPageOptedIntoEdgeToEdge Whether the page has opted into edge-to-edge.
     * @param layoutType The active layout type being shown.
     * @param bottomInset The bottom inset representing the height of the bottom OS navbar.
     * @return whether we should draw ToEdge based only on the given Tab and the viewport-fit value
     *     from the tracking data of the Display Cutout Controller.
     */
    static boolean shouldDrawToEdge(
            boolean isPageOptedIntoEdgeToEdge, @LayoutType int layoutType, int bottomInset) {
        return isPageOptedIntoEdgeToEdge
                || (isBottomChinFeatureEnabled() && isBottomChinAllowed(layoutType, bottomInset))
                || (isDrawKeyNativePageToEdgeEnabled()
                        && layoutType == LayoutType.TAB_SWITCHER
                        && !ChromeFeatureList.sDrawKeyNativeEdgeToEdgeDisableHubE2e.getValue());
    }

    /**
     * @param layoutType The active layout type being shown.
     * @param bottomInset The bottom inset representing the height of the bottom OS navbar.
     * @return Whether the bottom chin is allowed to be shown.
     */
    static boolean isBottomChinAllowed(@LayoutType int layoutType, int bottomInset) {
        boolean supportedLayoutType =
                layoutType == LayoutType.BROWSING
                        || layoutType == LayoutType.TOOLBAR_SWIPE
                        || layoutType == LayoutType.SIMPLE_ANIMATION;

        // Check that the bottom inset is greater than zero, otherwise there is no space to show the
        // bottom chin. A zero inset indicates a lack of "dismissable" bottom bar (e.g. fullscreen
        // mode, 3-button nav).
        boolean nonZeroEdgeToEdgeBottomInset = bottomInset > 0;

        return supportedLayoutType && nonZeroEdgeToEdgeBottomInset;
    }

    /**
     * @return whether the page is opted into edge-to-edge based on the given Tab
     */
    public static boolean isPageOptedIntoEdgeToEdge(@Nullable Tab tab) {
        if (tab == null || tab.isNativePage()) {
            return isNativeTabDrawingToEdge(tab);
        }
        if (tab.shouldEnableEmbeddedMediaExperience()) {
            return isDrawKeyNativePageToEdgeEnabled();
        }
        if (sAlwaysDrawWebEdgeToEdgeForTesting) {
            return true;
        }
        return isEdgeToEdgeWebOptInEnabled() && getWasViewportFitCover(tab);
    }

    /**
     * @return whether the page is opted into edge-to-edge based on the given Tab and the given new
     *     viewport-fit value.
     */
    static boolean isPageOptedIntoEdgeToEdge(
            @Nullable Tab tab, @WebContentsObserver.ViewportFitType int value) {
        if (tab == null || tab.isNativePage()) {
            return isNativeTabDrawingToEdge(tab);
        }
        if (sAlwaysDrawWebEdgeToEdgeForTesting) {
            return true;
        }
        if (tab.shouldEnableEmbeddedMediaExperience()) {
            return isDrawKeyNativePageToEdgeEnabled();
        }
        if (!isEdgeToEdgeWebOptInEnabled()) {
            return false;
        }
        return value == ViewportFit.COVER || value == ViewportFit.COVER_FORCED_BY_USER_AGENT;
    }

    /** Return whether there's any safe area constraint found for the given tab. */
    static boolean hasSafeAreaConstraintForTab(@Nullable Tab tab) {
        if (tab == null || !isSafeAreaConstraintEnabled()) return false;

        SafeAreaInsetsTracker safeAreaInsetsTracker =
                DisplayCutoutController.getSafeAreaInsetsTracker(tab);
        return safeAreaInsetsTracker != null && safeAreaInsetsTracker.hasSafeAreaConstraint();
    }

    /** Whether a native tab will be drawn edge to to edge. */
    static boolean isNativeTabDrawingToEdge(@Nullable Tab activeTab) {
        if (!isDrawKeyNativePageToEdgeEnabled()) return false;

        // TODO(crbug.com/339025702): Check if we are in tab switcher when activeTab is null.
        if (activeTab == null) return false;

        NativePage nativePage = activeTab.getNativePage();
        return nativePage != null && nativePage.supportsEdgeToEdge();
    }

    /**
     * @return whether the given window's insets indicate a tappable navigation bar.
     * @deprecated Use {@link #hasTappableNavigationBar(Supplier)}.
     */
    @Deprecated
    static boolean hasTappableNavigationBar(Window window) {
        Supplier<WindowInsetsCompat> insetsSupplier =
                () -> {
                    var rootInsets = window.getDecorView().getRootWindowInsets();
                    assert rootInsets != null;

                    return WindowInsetsCompat.toWindowInsetsCompat(rootInsets);
                };
        return hasTappableNavigationBar(insetsSupplier);
    }

    /**
     * @param insetsSupplier Supplier for the root window insets.
     * @return whether the given window's insets indicate a tappable navigation bar.
     */
    static boolean hasTappableNavigationBar(Supplier<WindowInsetsCompat> insetsSupplier) {
        if (sHas3ButtonNavBarForTesting != null) {
            return sHas3ButtonNavBarForTesting;
        }

        if (sObservedTappableNavigationBar
                && ChromeFeatureList.sEdgeToEdgeMonitorConfigurations.isEnabled()) {
            return true;
        }

        var rootInsets = insetsSupplier.get();
        assert rootInsets != null;

        boolean hasTappableNavBar = hasTappableNavigationBarFromInsets(rootInsets);
        sObservedTappableNavigationBar |= hasTappableNavBar;
        return hasTappableNavBar;
    }

    /** Returns whether the given window's insets contains a tappable navigation bar. */
    static boolean hasTappableNavigationBarFromInsets(WindowInsetsCompat insets) {
        Insets navigationBarInsets = insets.getInsets(WindowInsetsCompat.Type.navigationBars());
        Insets tappableElementInsets = insets.getInsets(WindowInsetsCompat.Type.tappableElement());
        // Return whether there is any overlap in navigation bar and tappable element insets.
        return (navigationBarInsets.bottom > 0 && tappableElementInsets.bottom > 0)
                || (navigationBarInsets.left > 0 && tappableElementInsets.left > 0)
                || (navigationBarInsets.right > 0 && tappableElementInsets.right > 0);
    }

    /**
     * @param insetsSupplier Supplier for the root window insets.
     * @return whether the given window's insets indicate a tappable bar, ignoring the top status
     *     bar inset.
     */
    static boolean hasTappableBarIgnoringTop(Supplier<WindowInsetsCompat> insetsSupplier) {
        if (sHas3ButtonNavBarForTesting != null) {
            return sHas3ButtonNavBarForTesting;
        }

        var rootInsets = insetsSupplier.get();
        assert rootInsets != null;

        return hasTappableBarFromInsetsIgnoringTop(rootInsets);
    }

    /**
     * Returns whether the given window's insets contains a tappable bar, ignoring the top status
     * bar insets.
     */
    static boolean hasTappableBarFromInsetsIgnoringTop(WindowInsetsCompat insets) {
        Insets tappableElementInsets = insets.getInsets(WindowInsetsCompat.Type.tappableElement());
        return tappableElementInsets.bottom > 0
                || tappableElementInsets.left > 0
                || tappableElementInsets.right > 0;
    }

    /**
     * Returns whether the given Tab has a web page that was already rendered with
     * viewport-fit=cover.
     */
    static boolean getWasViewportFitCover(Tab tab) {
        assert tab != null;
        SafeAreaInsetsTracker safeAreaInsetsTracker =
                DisplayCutoutController.getSafeAreaInsetsTracker(tab);
        return safeAreaInsetsTracker == null ? false : safeAreaInsetsTracker.isViewportFitCover();
    }

    public static void setAlwaysDrawWebEdgeToEdgeForTesting(boolean drawWebEdgeToEdge) {
        sAlwaysDrawWebEdgeToEdgeForTesting = drawWebEdgeToEdge;
        ResettersForTesting.register(() -> sAlwaysDrawWebEdgeToEdgeForTesting = false);
    }

    /** Whether push safe-area-insets-bottom to pages that's not using viewport-fit=cover. */
    public static boolean pushSafeAreaInsetsForNonOptInPages() {
        return ChromeFeatureList.sDynamicSafeAreaInsets.isEnabled();
    }

    public static void setObservedTappableNavigationBarForTesting(boolean observed) {
        sObservedTappableNavigationBar = observed;
        ResettersForTesting.register(() -> sObservedTappableNavigationBar = false);
    }

    public static void setHas3ButtonNavBarForTesting(Boolean has3ButtonNavBar) {
        sHas3ButtonNavBarForTesting = has3ButtonNavBar;
        ResettersForTesting.register(() -> sHas3ButtonNavBarForTesting = null);
    }

    /** Returns whether the insets indicate that the device is in gesture navigation mode. */
    public static boolean isInGestureNavigationMode(WindowInsetsCompat insets) {
        Insets mandatorySystemGesturesInsets =
                insets.getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        Insets systemGesturesInsets = insets.getInsets(WindowInsetsCompat.Type.systemGestures());
        Insets nonMandatorySystemGestures =
                Insets.subtract(systemGesturesInsets, mandatorySystemGesturesInsets);

        // In gesture navigation mode, the left and right sides have insets for swiping gestures,
        // but these are not considered mandatory system gestures. These non-mandatory gesture
        // insets do not appear in 3-button navigation mode. Note, though, that even in gesture
        // navigation mode, one side may not show an inset when in landscape mode, as the side with
        // the display cutout / camera will not show a gesture inset (the other side will still show
        // an inset).
        return nonMandatorySystemGestures.left > 0 || nonMandatorySystemGestures.right > 0;
    }

    /**
     * A class to store the debugging info for edge-to-edge error case, when EdgeToEdgeController is
     * presented in an unsupported configuration.
     */
    public static class EdgeToEdgeDebuggingInfo {
        boolean mHasUploaded;
        @Nullable @MissingNavbarInsetsReason Integer mMissingNavBarReason;
        @Nullable String mUploadMessage;
        final Callback<String> mReportUploadCallback;

        /**
         * Create an EdgeToEdgeDebuggingInfo for logging info useful to debugging edge-to-edge
         * issues.
         *
         * @param reportUploadCallback The callback to update debugging report.
         */
        public EdgeToEdgeDebuggingInfo(Callback<String> reportUploadCallback) {
            mReportUploadCallback = reportUploadCallback;
        }

        /**
         * Add to the current debug information
         *
         * @param callSite The call site of the debugging info.
         * @param hasEdgeToEdgeController Whether the activity has an EdgeToEdgeController.
         * @param isSupportedConfiguration Whether the activity supports for edge-to-edge.
         * @param window The current window.
         * @param windowAndroid The window android of the activity.
         */
        public void addToDebugReport(
                String callSite,
                boolean hasEdgeToEdgeController,
                boolean isSupportedConfiguration,
                @Nullable Window window,
                @Nullable WindowAndroid windowAndroid) {
            if (mHasUploaded || !ChromeFeatureList.sEdgeToEdgeDebugging.isEnabled()) return;
            if (!isCaseOfInterests(hasEdgeToEdgeController, window)) return;

            if (mUploadMessage == null) {
                mUploadMessage =
                        "EdgeToEdgeDebugging:"
                                + " \nedgeToEdgeEverywhere: "
                                + EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled()
                                + " \nmissingNavBarReason: "
                                + mMissingNavBarReason;
            }

            String hasSeenNonZeroNavBar = "";
            String rawWindowInsetsIndicateGestureNav = "";
            String rawWindowInsetsState = "";
            String rawWindowInsetsIgnoringVisibilityState = "";
            String rawWindowInsetsTappableState = "";
            String rawWindowInsetsSystemGesturesState = "";
            if (windowAndroid != null && windowAndroid.getInsetObserver() != null) {
                hasSeenNonZeroNavBar =
                        " \nhasSeenNonZeroNavBar: "
                                + windowAndroid
                                        .getInsetObserver()
                                        .hasSeenNonZeroNavigationBarInsets();

                var lastRawWindowInsets = windowAndroid.getInsetObserver().getLastRawWindowInsets();
                var gestureNavString =
                        lastRawWindowInsets == null
                                ? "null"
                                : Boolean.toString(
                                        EdgeToEdgeUtils.isInGestureNavigationMode(
                                                lastRawWindowInsets));
                rawWindowInsetsIndicateGestureNav =
                        "\nisGestureNav (from lastRawWindowInsets): " + gestureNavString;

                var insetsString =
                        lastRawWindowInsets == null
                                ? "null"
                                : lastRawWindowInsets
                                        .getInsets(WindowInsetsCompat.Type.systemBars())
                                        .toString();
                rawWindowInsetsState = " \nlastRawWindowInsets: " + insetsString;

                var insetsIgnoringVisibilityString =
                        lastRawWindowInsets == null
                                ? "null"
                                : lastRawWindowInsets
                                        .getInsetsIgnoringVisibility(
                                                WindowInsetsCompat.Type.systemBars())
                                        .toString();
                rawWindowInsetsIgnoringVisibilityState =
                        " \nlastRawWindowInsets ignoringVisibility: "
                                + insetsIgnoringVisibilityString;

                var tappableInsetsString =
                        lastRawWindowInsets == null
                                ? "null"
                                : lastRawWindowInsets
                                        .getInsetsIgnoringVisibility(
                                                WindowInsetsCompat.Type.tappableElement())
                                        .toString();
                rawWindowInsetsTappableState =
                        " \nlastRawWindowInsets tappable: " + tappableInsetsString;

                var systemGesturesInsetsString =
                        lastRawWindowInsets == null
                                ? "null"
                                : lastRawWindowInsets
                                        .getInsetsIgnoringVisibility(
                                                WindowInsetsCompat.Type.systemGestures())
                                        .toString();
                rawWindowInsetsSystemGesturesState =
                        " \nlastRawWindowInsets systemGestures: " + systemGesturesInsetsString;
            }

            String windowMetricsIndicateGestureNav = "";
            String windowMetricsInsetsState = "";
            String windowMetricsInsetsStateTappable = "";
            String windowMetricsInsetsStateSystemGestures = "";
            if (Build.VERSION.SDK_INT >= VERSION_CODES.R) {
                if (window != null
                        && window.getWindowManager() != null
                        && window.getWindowManager().getCurrentWindowMetrics() != null) {
                    WindowInsets windowInsets =
                            window.getWindowManager().getCurrentWindowMetrics().getWindowInsets();

                    var gestureNavString =
                            windowInsets == null
                                    ? "null"
                                    : Boolean.toString(
                                            EdgeToEdgeUtils.isInGestureNavigationMode(
                                                    WindowInsetsCompat.toWindowInsetsCompat(
                                                            windowInsets)));
                    windowMetricsIndicateGestureNav =
                            "\nisGestureNav (from windowMetrics): " + gestureNavString;

                    var insetsString =
                            windowInsets == null
                                    ? "null"
                                    : windowInsets
                                            .getInsets(WindowInsets.Type.systemBars())
                                            .toString();
                    windowMetricsInsetsState = " \nwindowMetricsInsets: " + insetsString;

                    var insetsStringTappable =
                            windowInsets == null
                                    ? "null"
                                    : windowInsets
                                            .getInsets(WindowInsets.Type.tappableElement())
                                            .toString();
                    windowMetricsInsetsStateTappable =
                            " \nwindowMetricsInsetsTappable: " + insetsStringTappable;

                    var insetsStringSystemGestures =
                            windowInsets == null
                                    ? "null"
                                    : windowInsets
                                            .getInsets(WindowInsets.Type.systemGestures())
                                            .toString();
                    windowMetricsInsetsStateSystemGestures =
                            " \nwindowMetricsInsetsSystemGestures: " + insetsStringSystemGestures;
                }
            }
            mUploadMessage +=
                    "\nand\ncallSite: "
                            + callSite
                            + "\nhasSeenNonZeroNavBar"
                            + hasSeenNonZeroNavBar
                            + "\nisSupportedConfiguration: "
                            + isSupportedConfiguration
                            + " \nobservedTappableNavigationBar: "
                            + sObservedTappableNavigationBar
                            + rawWindowInsetsIndicateGestureNav
                            + rawWindowInsetsState
                            + rawWindowInsetsIgnoringVisibilityState
                            + rawWindowInsetsTappableState
                            + rawWindowInsetsSystemGesturesState
                            + windowMetricsIndicateGestureNav
                            + windowMetricsInsetsState
                            + windowMetricsInsetsStateTappable
                            + windowMetricsInsetsStateSystemGestures;
        }

        /** Uploads the current report. */
        public void uploadReport() {
            if (mHasUploaded || !ChromeFeatureList.sEdgeToEdgeDebugging.isEnabled()) return;
            if (mUploadMessage == null) return;

            // Ensure report is only sent once.
            mHasUploaded = true;
            mReportUploadCallback.onResult(mUploadMessage);
        }

        /** Returns whether the the instance has uploaded any report. */
        public boolean isUsed() {
            return mHasUploaded;
        }

        /** Set the reason why nav bar insets is missing when controller is initialized. */
        public void setMissingNavBarInsetsReason(@MissingNavbarInsetsReason int reason) {
            mMissingNavBarReason = reason;
        }

        /** Whether the current state contains useful debug information. */
        boolean isCaseOfInterests(boolean hasEdgeToEdgeController, @Nullable Window window) {
            if (window == null || window.getDecorView().getRootWindowInsets() == null) {
                return false;
            }

            // Case of interest:
            // a) When e2e controller is created but tappable navigation bar is observed.
            boolean hasTappableNavBarAndController =
                    hasEdgeToEdgeController
                            && hasTappableNavigationBarFromInsets(
                                    WindowInsetsCompat.toWindowInsetsCompat(
                                            window.getDecorView().getRootWindowInsets()));

            // b) controller created due to an empty insets being presented.
            boolean missingNavBarOnControllerCreation =
                    hasEdgeToEdgeController && mMissingNavBarReason != null;
            return hasTappableNavBarAndController || missingNavBarOnControllerCreation;
        }
    }
}
