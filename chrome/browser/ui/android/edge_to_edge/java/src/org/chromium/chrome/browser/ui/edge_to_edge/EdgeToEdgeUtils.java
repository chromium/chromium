// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.app.Activity;
import android.os.Build.VERSION_CODES;
import android.view.Window;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController.SafeAreaInsetsTracker;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.components.cached_flags.StringCachedFieldTrialParameter;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A util helper class to know if e2e is on and eligible for current session and to record metrics
 * when necessary.
 */
public class EdgeToEdgeUtils {
    private static boolean sAlwaysDrawWebEdgeToEdgeForTesting;

    private static final String ELIGIBLE_HISTOGRAM = "Android.EdgeToEdge.Eligible";
    private static final String INELIGIBLE_REASON_HISTOGRAM =
            "Android.EdgeToEdge.IneligibilityReason";

    private static final String PARAM_DISABLE_INCOGNITO_NTP_E2E = "disable_incognito_ntp_e2e";

    /** Cached param whether we disable e2e on incognito new tab page. See crbug.com/368675202 */
    public static BooleanCachedFieldTrialParameter DISABLE_INCOGNITO_NTP_E2E =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
                    PARAM_DISABLE_INCOGNITO_NTP_E2E,
                    false);

    private static final String PARAM_DISABLE_NTP_E2E = "disable_ntp_e2e";

    /** Cached param whether we disable e2e on new tab page. */
    public static BooleanCachedFieldTrialParameter DISABLE_NTP_E2E =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE, PARAM_DISABLE_NTP_E2E, false);

    private static final String PARAM_DISABLE_HUB_E2E = "disable_hub_e2e";

    /** Cached param whether we disable e2e on the hub. */
    public static BooleanCachedFieldTrialParameter DISABLE_HUB_E2E =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE, PARAM_DISABLE_HUB_E2E, false);

    private static final String PARAM_DISABLE_CCT_MEDIA_VIEWER_E2E = "disable_cct_media_viewer_e2e";

    /** Cached param whether we disable e2e on the CCT media viewer. */
    public static BooleanCachedFieldTrialParameter DISABLE_CCT_MEDIA_VIEWER_E2E =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
                    PARAM_DISABLE_CCT_MEDIA_VIEWER_E2E,
                    false);

    private static final String PARAM_E2E_FIELD_TRIAL_OEM_LIST = "e2e_field_trial_oem_list";
    private static final String PARAM_E2E_FIELD_TRIAL_OEM_MIN_VERSIONS =
            "e2e_field_trial_oem_min_versions";

    /**
     * Param for the OEMs that need an exception for min versions. Its value should be a comma
     * separated list of manufacturer, and its index should match {@link
     * #E2E_FIELD_TRIAL_OEM_MIN_VERSIONS}.
     */
    public static StringCachedFieldTrialParameter E2E_FIELD_TRIAL_OEM_LIST =
            ChromeFeatureList.newStringCachedFieldTrialParameter(
                    ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN, PARAM_E2E_FIELD_TRIAL_OEM_LIST, "");

    /**
     * Param for the OEMs that need an exception for min versions. Its value should be a comma
     * separated list of integers, and its index should match {@link #E2E_FIELD_TRIAL_OEM_LIST}.
     */
    public static StringCachedFieldTrialParameter E2E_FIELD_TRIAL_OEM_MIN_VERSIONS =
            ChromeFeatureList.newStringCachedFieldTrialParameter(
                    ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
                    PARAM_E2E_FIELD_TRIAL_OEM_MIN_VERSIONS,
                    "");

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

    /**
     * Whether the draw edge to edge infrastructure is on. When this is enabled, Chrome will start
     * drawing edge to edge on start up.
     */
    public static boolean isEnabled() {
        return isLegacyWebsiteOptInEnabled()
                || isEdgeToEdgeBottomChinEnabled()
                || isEdgeToEdgeWebOptInEnabled()
                || isEdgeToEdgeEverywhereEnabled();
    }

    /**
     * Whether drawing website opt-in is enabled.
     *
     * <p>When enabled, Chrome will add bottom padding to the root view if the current tab / UI is
     * not a tab with `viewport-fit=cover`. Additionally, bottom attached UI will be padded to avoid
     * drawing into the bottom navigation bar region.
     *
     * @deprecated This method will be removed. External references should use {@link #isEnabled()}.
     */
    public static boolean isLegacyWebsiteOptInEnabled() {
        return ChromeFeatureList.sDrawEdgeToEdge.isEnabled();
    }

    /**
     * Whether the edge-to-edge bottom chin is enabled.
     *
     * <p>When enabled, Chrome will replace the OS navigation bar with a thin "Chin" layer in the
     * browser controls and can be scrolled off the screen on web pages.
     */
    public static boolean isEdgeToEdgeBottomChinEnabled() {
        return ChromeFeatureList.sEdgeToEdgeBottomChin.isEnabled();
    }

    /**
     * Whether drawing the website that has `viewport-fit=cover` fully edge to edge, removing the
     * bottom chin.
     */
    public static boolean isEdgeToEdgeWebOptInEnabled() {
        return ChromeFeatureList.sEdgeToEdgeWebOptIn.isEnabled();
    }

    /** Whether edge-to-edge should be enabled everywhere. */
    public static boolean isEdgeToEdgeEverywhereEnabled() {
        return ChromeFeatureList.sEdgeToEdgeEverywhere.isEnabled();
    }

    /** Whether key native pages should draw to edge. */
    public static boolean isDrawKeyNativePageToEdgeEnabled() {
        return isEnabled() && ChromeFeatureList.sDrawKeyNativeEdgeToEdge.isEnabled();
    }

    /**
     * Record if the current activity is eligible for edge to edge. If not, also record the reason
     * why it is ineligible.
     *
     * @param activity The current active activity.
     * @return Whether the activity is eligible for edge to edge based on device configuration.
     */
    public static boolean recordEligibility(@NonNull Activity activity) {
        boolean eligible = true;

        if (hasTappableBottomBar(activity.getWindow())) {
            eligible = false;
            RecordHistogram.recordEnumeratedHistogram(
                    INELIGIBLE_REASON_HISTOGRAM,
                    IneligibilityReason.NAVIGATION_MODE,
                    IneligibilityReason.NUM_TYPES);
        }

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)) {
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
     * @param isPageOptedIntoEdgeToEdge Whether the page has opted into edge-to-edge.
     * @param layoutType The active layout type being shown.
     * @param bottomInset The bottom inset representing the height of the bottom OS navbar.
     * @return whether we should draw ToEdge based only on the given Tab and the viewport-fit value
     *     from the tracking data of the Display Cutout Controller.
     */
    static boolean shouldDrawToEdge(
            boolean isPageOptedIntoEdgeToEdge, @LayoutType int layoutType, int bottomInset) {
        return isPageOptedIntoEdgeToEdge
                || (isEdgeToEdgeBottomChinEnabled() && isBottomChinAllowed(layoutType, bottomInset))
                || (isDrawKeyNativePageToEdgeEnabled()
                        && layoutType == LayoutType.TAB_SWITCHER
                        && !DISABLE_HUB_E2E.getValue());
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
    public static boolean isPageOptedIntoEdgeToEdge(Tab tab) {
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
            Tab tab, @WebContentsObserver.ViewportFitType int value) {
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

    /** Whether a native tab will be drawn edge to to edge. */
    static boolean isNativeTabDrawingToEdge(Tab activeTab) {
        // sDrawNativeEdgeToEdge will draw all native page to edge forcefully.
        if (ChromeFeatureList.sDrawNativeEdgeToEdge.isEnabled()) return true;

        if (!ChromeFeatureList.sDrawKeyNativeEdgeToEdge.isEnabled()) return false;

        // TODO(crbug.com/339025702): Check if we are in tab switcher when activeTab is null.
        if (activeTab == null) return false;

        NativePage nativePage = activeTab.getNativePage();
        return nativePage != null && nativePage.supportsEdgeToEdge();
    }

    /**
     * @return whether the given window's insets indicate a tappable bottom bar.
     */
    static boolean hasTappableBottomBar(Window window) {
        var rootInsets = window.getDecorView().getRootWindowInsets();
        assert rootInsets != null;
        return WindowInsetsCompat.toWindowInsetsCompat(rootInsets)
                        .getInsets(WindowInsetsCompat.Type.tappableElement())
                        .bottom
                != 0;
    }

    /**
     * Returns whether the given Tab has a web page that was already rendered with
     * viewport-fit=cover.
     */
    static boolean getWasViewportFitCover(@NonNull Tab tab) {
        assert tab != null;
        SafeAreaInsetsTracker safeAreaInsetsTracker =
                DisplayCutoutController.getSafeAreaInsetsTracker(tab);
        return safeAreaInsetsTracker == null ? false : safeAreaInsetsTracker.isViewportFitCover();
    }

    public static void setAlwaysDrawWebEdgeToEdgeForTesting(boolean drawWebEdgeToEdge) {
        sAlwaysDrawWebEdgeToEdgeForTesting = drawWebEdgeToEdge;
        ResettersForTesting.register(() -> sAlwaysDrawWebEdgeToEdgeForTesting = false);
    }
}
