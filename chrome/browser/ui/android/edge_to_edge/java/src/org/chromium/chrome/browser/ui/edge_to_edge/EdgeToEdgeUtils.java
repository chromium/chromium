// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.app.Activity;
import android.os.Build.VERSION_CODES;
import android.view.Window;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController.SafeAreaInsetsTracker;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A util helper class to know if e2e is on and eligible for current session and to record metrics
 * when necessary.
 */
public class EdgeToEdgeUtils {

    private static final String ELIGIBLE_HISTOGRAM = "Android.EdgeToEdge.Eligible";
    private static final String INELIGIBLE_REASON_HISTOGRAM =
            "Android.EdgeToEdge.IneligibilityReason";

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
     * @return True if the draw edge to edge infrastructure is on.
     */
    public static boolean isEnabled() {
        return ChromeFeatureList.sDrawEdgeToEdge.isEnabled();
    }

    /**
     * Record if the current activity is eligible for edge to edge. If not, also record the reason
     * why it is ineligible.
     *
     * @param activity The current active activity.
     */
    public static void recordEligibility(@NonNull Activity activity) {
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
    }

    /**
     * @return whether we should draw ToEdge based only on the given Tab and the viewport-fit value
     *     from the tracking data of the Display Cutout Controller.
     */
    public static boolean shouldDrawToEdge(Tab tab) {
        return shouldDrawToEdge(
                tab,
                tab == null
                        ? ChromeFeatureList.sDrawNativeEdgeToEdge.isEnabled()
                        : getWasViewportFitCover(tab));
    }

    /**
     * @return whether we should draw ToEdge based on the given Tab and the given new viewport-fit
     *     value.
     */
    static boolean shouldDrawToEdge(Tab tab, @WebContentsObserver.ViewportFitType int value) {
        return shouldDrawToEdge(
                tab, value == ViewportFit.COVER || value == ViewportFit.COVER_FORCED_BY_USER_AGENT);
    }

    /**
     * @return whether we should draw ToEdge based on the given Tab and a ToEdge preference boolean.
     */
    static boolean shouldDrawToEdge(Tab tab, boolean wantsToEdge) {
        // The calling infrastructure has already checked the device configuration: mobile vs tablet
        // and whether the Gesture Navigation is appropriately enabled or not.
        if (alwaysDrawToEdgeForTabKind(tab)) return true;
        return wantsToEdge;
    }

    /**
     * @return whether the given window's insets indicate a tappable bottom bar.
     */
    static boolean hasTappableBottomBar(Window window) {
        return WindowInsetsCompat.toWindowInsetsCompat(window.getDecorView().getRootWindowInsets())
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

    /**
     * Decides whether to draw the given Tab ToEdge or not.
     *
     * @param tab The {@link Tab} to be drawn.
     * @return {@code true} if it's OK to draw this Tab under system bars.
     */
    static boolean alwaysDrawToEdgeForTabKind(@Nullable Tab tab) {
        boolean isNative = tab == null || tab.isNativePage();
        if (isNative) {
            // Check the flag for ToEdge on all native pages.
            return ChromeFeatureList.sDrawNativeEdgeToEdge.isEnabled();
        }
        return ChromeFeatureList.sDrawWebEdgeToEdge.isEnabled();
    }

    /** Whether enable the inset management through InsetObserver. */
    public static boolean isInsetsManagementEnabled() {
        return ChromeFeatureList.sDrawEdgeToEdgeInsetsManagement.isEnabled();
    }
}
