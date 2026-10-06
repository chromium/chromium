// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.Window;

import androidx.annotation.IntDef;
import androidx.annotation.OptIn;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.os.BuildCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.ApkInfo;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController.SafeAreaInsetsTracker;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayUtil;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

/**
 * A util helper class to know if e2e is on and eligible for current session and to record metrics
 * when necessary.
 */
@NullMarked
public class EdgeToEdgeUtils {
    private static final String TAG = "E2E_Utils";
    private static @Nullable Boolean sIsTargetSdkEnforceEdgeToEdge;
    private static boolean sAlwaysDrawWebEdgeToEdgeForTesting;
    private static @Nullable Boolean sHas3ButtonNavBarForTesting;

    private static final String ELIGIBLE_HISTOGRAM = "Android.EdgeToEdge.Eligible2";
    private static final String INELIGIBLE_REASON_HISTOGRAM =
            "Android.EdgeToEdge.IneligibilityReason2";
    private static final String ELIGIBLE_ON_CREATE_HISTOGRAM =
            "Android.EdgeToEdge.Eligible2.OnCreateController";
    private static final String INELIGIBLE_REASON_ON_CREATE_HISTOGRAM =
            "Android.EdgeToEdge.IneligibilityReason2.OnCreateController";
    private static final String MISSING_NAVBAR_INSETS_HISTOGRAM =
            "Android.EdgeToEdge.MissingNavbarInsets2";
    private static final String DRAW_TO_EDGE_UNSUPPORTED_CONFIG_HISTOGRAM =
            "Android.EdgeToEdge.DrawToEdgeInUnsupportedConfiguration";
    private static final String SUPPORTED_CONFIGURATION_SWITCH_HISTOGRAM =
            "Android.EdgeToEdge.SupportedConfigurationSwitch2";
    private static final String CONFIGURATION_SWITCH_OUTCOME_HISTOGRAM =
            "Android.EdgeToEdge.Debugging.ConfigurationSwitchOutcome";
    private static final String SUPPORTED_CONFIGURATION_STRANGE_INSETS_HISTOGRAM =
            "Android.EdgeToEdge.Debugging.SupportedConfigurationStrangeInsets";

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

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(SupportedConfigurationSwitch)
    @IntDef({
        SupportedConfigurationSwitch.FROM_SUPPORTED_TO_UNSUPPORTED,
        SupportedConfigurationSwitch.FROM_UNSUPPORTED_TO_SUPPORTED,
        SupportedConfigurationSwitch.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface SupportedConfigurationSwitch {
        int FROM_SUPPORTED_TO_UNSUPPORTED = 0;
        int FROM_UNSUPPORTED_TO_SUPPORTED = 1;
        int NUM_ENTRIES = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:SupportedConfigurationSwitch)

    /** When configuration changes from supported to unsupported, what's the outcome */
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(ConfigurationSwitchOutcome)
    @IntDef({
        ConfigurationSwitchOutcome.ADD_PADDING_NEW_INSETS,
        ConfigurationSwitchOutcome.ADD_PADDING_ORIGINAL_INSETS,
        ConfigurationSwitchOutcome.ERROR_ADD_PADDING_BOTH_INSETS_EMPTY,
        ConfigurationSwitchOutcome.NO_PADDING_BOTH_INSETS_EMPTY,
        ConfigurationSwitchOutcome.NO_PADDING_NO_NEW_INSETS,
        ConfigurationSwitchOutcome.ERROR_NO_PADDING_WITH_NEW_INSETS,
        ConfigurationSwitchOutcome.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ConfigurationSwitchOutcome {

        // Correct cases
        int ADD_PADDING_NEW_INSETS = 0;
        int ADD_PADDING_ORIGINAL_INSETS = 1;
        // Error case / impossible case
        int ERROR_ADD_PADDING_BOTH_INSETS_EMPTY = 2;
        int NO_PADDING_BOTH_INSETS_EMPTY = 3;
        int NO_PADDING_NO_NEW_INSETS = 4;
        // Error case / impossible case
        int ERROR_NO_PADDING_WITH_NEW_INSETS = 5;

        int NUM_ENTRIES = 6;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:EdgeToEdgeConfigurationSwitchOutcome)

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(SupportedConfigurationStrangeInsetsState)
    @IntDef({
        SupportedConfigurationStrangeInsetsState.TAPPABLE_ELEMENT_NOT_GESTURE_NAV,
        SupportedConfigurationStrangeInsetsState.NO_TAPPABLE_ELEMENT_NOT_GESTURE_NAV,
        SupportedConfigurationStrangeInsetsState.ERROR_TAPPABLE_ELEMENT_GESTURE_NAV,
        SupportedConfigurationStrangeInsetsState.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface SupportedConfigurationStrangeInsetsState {
        int TAPPABLE_ELEMENT_NOT_GESTURE_NAV = 0;
        int NO_TAPPABLE_ELEMENT_NOT_GESTURE_NAV = 1;
        int ERROR_TAPPABLE_ELEMENT_GESTURE_NAV = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:SupportedConfigurationStrangeInsetsState)

    /** Whether it is allowed to use other insets as a backup for missing navigation bar insets. */
    public static boolean isUseBackupNavbarInsetsEnabled() {
        return ChromeFeatureList.sEdgeToEdgeUseBackupNavbarInsets.isEnabled();
    }

    /**
     * Returns whether the configuration of the device should allow Edge To Edge bottom chin. Note
     * the results are false-negative, if the method is called before the |activity|'s decor view
     * being attached to the window.
     */
    public static boolean isEdgeToEdgeBottomChinSupportedByDevice(Activity activity) {
        // Make sure we test SDK version before checking the Feature so Field Trials only collect
        // from qualifying devices.
        if (!EdgeToEdgeFieldTrialImpl.getBottomChinOverrides().isEnabledForManufacturerVersion()) {
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
                && (!isEdgeToEdgeTabletEnabled()
                        || !isBottomEdgeToEdgeSupportedOnTablet(activity))) {
            return false;
        }

        return !DeviceInfo.isAutomotive() && !hasTappableNavigationBar(activity.getWindow());
    }

    /** Whether the edge-to-edge feature is enabled on automotive. */
    public static boolean isEdgeToEdgeAutomotiveEnabled() {
        return ChromeFeatureList.sEdgeToEdgeAutomotive.isEnabled();
    }

    /**
     * This is a sensitive check for whether all insets indicate or imply that the device is in
     * gesture navigation mode, and not tappable (3-button) navigation mode.
     *
     * @param insets The window insets to check for signals indicating gesture navigation.
     * @return Whether all insets indicate the device is in gesture navigation mode.
     */
    public static boolean doAllInsetsIndicateGestureNavigation(
            @Nullable WindowInsetsCompat insets) {
        return insets != null
                && isInGestureNavigationMode(insets)
                && !hasTappableBarIgnoringTop(() -> insets);
    }

    /** Whether the edge-to-edge feature is enabled on tablet. */
    public static boolean isEdgeToEdgeTabletEnabled() {
        return ChromeFeatureList.sEdgeToEdgeTablet.isEnabled();
    }

    /**
     * Returns whether the tablet's smallest screen width meets the minimum width threshold ({@code
     * MinWidthThreshold}) for bottom edge-to-edge.
     *
     * <ul>
     *   <li>width < MinWidthThreshold: bottom e2e disabled.
     *   <li>MinWidthThreshold <= width < InvisibleBottomChinMinWidth: bottom e2e enabled and the
     *       bottom chin is visible by default. Same as behavior on phone.
     *   <li>InvisibleBottomChinMinWidth <= width: fully e2e and the bottom chin is invisible by
     *       default.
     * </ul>
     */
    public static boolean isBottomEdgeToEdgeSupportedOnTablet(Context context) {
        int widthThreshold = ChromeFeatureList.sEdgeToEdgeTabletMinWidthThreshold.getValue();
        if (widthThreshold == -1) {
            return true;
        }
        return DisplayUtil.getCurrentSmallestScreenWidth(context) >= widthThreshold;
    }

    /**
     * Returns {@code true} when the tablet's smallest screen width is below {@code
     * InvisibleBottomChinMinWidth} (meaning the bottom chin is visible by default like on phones,
     * whereas wider tablets hide the bottom chin by default).
     */
    public static boolean shouldShowBottomChinByDefaultOnTablet(Context context) {
        int widthThreshold =
                ChromeFeatureList.sEdgeToEdgeTabletInvisibleBottomChinMinWidth.getValue();
        if (widthThreshold == -1) {
            return false;
        }
        return DisplayUtil.getCurrentSmallestScreenWidth(context) < widthThreshold;
    }

    /** Whether edge-to-edge should be enabled everywhere. */
    @OptIn(markerClass = BuildCompat.PrereleaseSdkCheck.class)
    public static boolean isEdgeToEdgeEverywhereEnabled() {
        if (!EdgeToEdgeFieldTrialImpl.getEverywhereOverrides().isEnabledForManufacturerVersion()) {
            return false;
        }

        if (DeviceInfo.isAutomotive() && !isEdgeToEdgeAutomotiveEnabled()) {
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

    /**
     * Record if the current activity is eligible for edge to edge. If not, also record the reason
     * why it is ineligible. This is for the general "for all users" check at startup.
     *
     * @param activity The current active activity.
     * @return Whether the activity is eligible for edge to edge based on device configuration.
     */
    public static boolean recordEligibilityForEveryStart(Activity activity) {
        return recordEligibility(activity, ELIGIBLE_HISTOGRAM, INELIGIBLE_REASON_HISTOGRAM);
    }

    /**
     * Record if the current activity is eligible for edge to edge when the controller is created.
     *
     * @param activity The current active activity.
     * @return Whether the activity is eligible for edge to edge based on device configuration.
     */
    public static boolean recordEligibilityOnCreate(Activity activity) {
        return recordEligibility(
                activity, ELIGIBLE_ON_CREATE_HISTOGRAM, INELIGIBLE_REASON_ON_CREATE_HISTOGRAM);
    }

    /**
     * Checks if the current activity is eligible for edge to edge.
     *
     * @param activity The current active activity.
     * @param eligibleName The name of the histogram to record eligibility.
     * @param ineligibleName The name of the histogram to record ineligibility reasons.
     * @return Whether the activity is eligible for edge to edge based on device configuration.
     */
    private static boolean recordEligibility(
            Activity activity, String eligibleName, String ineligibleName) {
        boolean eligible = true;

        if (hasTappableNavigationBar(activity.getWindow())) {
            eligible = false;
            RecordHistogram.recordEnumeratedHistogram(
                    ineligibleName,
                    IneligibilityReason.NAVIGATION_MODE,
                    IneligibilityReason.NUM_TYPES);
        }

        // Not supported on tablet unless the flag is on and it meets the minimum screen size.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)
                && (!isEdgeToEdgeTabletEnabled()
                        || !isBottomEdgeToEdgeSupportedOnTablet(activity))) {
            eligible = false;
            RecordHistogram.recordEnumeratedHistogram(
                    ineligibleName, IneligibilityReason.FORM_FACTOR, IneligibilityReason.NUM_TYPES);
        }

        if (Build.VERSION.SDK_INT < VERSION_CODES.R) {
            eligible = false;
            RecordHistogram.recordEnumeratedHistogram(
                    ineligibleName, IneligibilityReason.OS_VERSION, IneligibilityReason.NUM_TYPES);
        }

        if (DeviceInfo.isAutomotive() && !isEdgeToEdgeAutomotiveEnabled()) {
            eligible = false;
            RecordHistogram.recordEnumeratedHistogram(
                    ineligibleName, IneligibilityReason.DEVICE_TYPE, IneligibilityReason.NUM_TYPES);
        }

        RecordHistogram.recordBooleanHistogram(eligibleName, eligible);

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
     * Record if drawToEdge is called when in an unsupported configuration.
     *
     * @param changedWindowState Whether drawToEdge was called due to window state change.
     */
    static void recordDrawToEdgeInUnsupportedConfig(boolean changedWindowState) {
        RecordHistogram.recordBooleanHistogram(
                DRAW_TO_EDGE_UNSUPPORTED_CONFIG_HISTOGRAM, changedWindowState);
    }

    /**
     * Record when the activity switches between supported and unsupported configurations.
     *
     * @param isSupportedConfiguration Whether the new configuration is supported.
     */
    static void recordSupportedConfigurationSwitch(boolean isSupportedConfiguration) {
        @SupportedConfigurationSwitch
        int configurationChanged =
                isSupportedConfiguration
                        ? SupportedConfigurationSwitch.FROM_UNSUPPORTED_TO_SUPPORTED
                        : SupportedConfigurationSwitch.FROM_SUPPORTED_TO_UNSUPPORTED;
        RecordHistogram.recordEnumeratedHistogram(
                SUPPORTED_CONFIGURATION_SWITCH_HISTOGRAM,
                configurationChanged,
                SupportedConfigurationSwitch.NUM_ENTRIES);
    }

    /**
     * Verify whether window insets in a supported configuration contain unexpected tappable
     * elements or non-gesture navigation insets, and record a histogram if so.
     *
     * @param windowInsets The window insets to check.
     */
    static void verifyInsetsInSupportedConfiguration(WindowInsetsCompat windowInsets) {
        // Check for the presence of a tappable element (in case the navigation bar inset is
        // missing for some reason) for logging purposes.
        Insets tappableElementInsets =
                windowInsets.getInsets(WindowInsetsCompat.Type.tappableElement());
        // The navigation bar will never be at the top.
        boolean tappableElement =
                tappableElementInsets.bottom > 0
                        || tappableElementInsets.left > 0
                        || tappableElementInsets.right > 0;

        // Check whether the device appears to be in gesture navigation mode.
        boolean isGestureNavigation = isInGestureNavigationMode(windowInsets);
        @SupportedConfigurationStrangeInsetsState int state;
        if (tappableElement) {
            if (isGestureNavigation) {
                state = SupportedConfigurationStrangeInsetsState.ERROR_TAPPABLE_ELEMENT_GESTURE_NAV;
            } else {
                state = SupportedConfigurationStrangeInsetsState.TAPPABLE_ELEMENT_NOT_GESTURE_NAV;
            }
        } else {
            if (isGestureNavigation) {
                // !tappableElement && isGestureNavigation is intended
                return;
            } else {
                state =
                        SupportedConfigurationStrangeInsetsState
                                .NO_TAPPABLE_ELEMENT_NOT_GESTURE_NAV;
            }
        }
        RecordHistogram.recordEnumeratedHistogram(
                SUPPORTED_CONFIGURATION_STRANGE_INSETS_HISTOGRAM,
                state,
                SupportedConfigurationStrangeInsetsState.NUM_ENTRIES);
    }

    /**
     * Record the padding outcome when switching from a supported configuration to an unsupported
     * configuration.
     */
    static void recordConfigurationSwitchScenario(
            Insets originalInsets, Insets newInsets, Insets paddingApplied) {
        // Do not record when configuration change is disabled.
        if (!ChromeFeatureList.sEdgeToEdgeMonitorConfigurations.isEnabled()) return;

        // Do not record landscape mode. Assuming the configuration change will be triggered
        // mostly with nav bar in portrait mode.
        if (paddingApplied.left > 0 || paddingApplied.right > 0) return;

        @ConfigurationSwitchOutcome int outcome;
        // Correct cases - fixed applied
        if (paddingApplied.bottom > 0) {
            if (originalInsets.bottom != 0) {
                outcome = ConfigurationSwitchOutcome.ADD_PADDING_ORIGINAL_INSETS;
            } else if (newInsets.bottom != 0) {
                outcome = ConfigurationSwitchOutcome.ADD_PADDING_NEW_INSETS;
            } else {
                outcome = ConfigurationSwitchOutcome.ERROR_ADD_PADDING_BOTH_INSETS_EMPTY;
            }
        } else { // paddingApplied.bottom == 0
            if (originalInsets.bottom == 0 && newInsets.bottom == 0) {
                outcome = ConfigurationSwitchOutcome.NO_PADDING_BOTH_INSETS_EMPTY;
            } else if (originalInsets.bottom > 0) {
                outcome = ConfigurationSwitchOutcome.NO_PADDING_NO_NEW_INSETS;
            } else {
                outcome = ConfigurationSwitchOutcome.ERROR_NO_PADDING_WITH_NEW_INSETS;
            }
        }

        RecordHistogram.recordEnumeratedHistogram(
                CONFIGURATION_SWITCH_OUTCOME_HISTOGRAM,
                outcome,
                ConfigurationSwitchOutcome.NUM_ENTRIES);
    }

    /**
     * @param isPageOptedIntoBottomEdgeToEdge Whether the page has opted into bottom edge-to-edge.
     * @param layoutType The active layout type being shown.
     * @param bottomInset The bottom inset representing the height of the bottom OS navbar.
     * @return whether we should draw to the bottom edge based on the given page opt-in status,
     *     active layout type, and bottom inset.
     */
    static boolean shouldDrawToBottomEdge(
            boolean isPageOptedIntoBottomEdgeToEdge, @LayoutType int layoutType, int bottomInset) {
        return isPageOptedIntoBottomEdgeToEdge
                || isBottomChinAllowed(layoutType, bottomInset)
                || (layoutType == LayoutType.HUB);
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
     * Returns whether the page is opted into bottom edge-to-edge based on the given Tab.
     *
     * @param tab The tab to check.
     */
    public static boolean isPageOptedIntoBottomEdgeToEdge(@Nullable Tab tab) {
        if (tab == null || tab.isNativePage()) {
            return isNativeTabDrawingToBottomEdge(tab);
        }
        if (sAlwaysDrawWebEdgeToEdgeForTesting || tab.shouldEnableEmbeddedMediaExperience()) {
            return true;
        }
        return getWasViewportFitCover(tab);
    }

    /**
     * Returns whether the page is opted into bottom edge-to-edge based on the given Tab and the
     * given new viewport-fit value.
     *
     * @param tab The tab to check.
     * @param value The new viewport-fit value of the root frame.
     */
    static boolean isPageOptedIntoBottomEdgeToEdge(
            @Nullable Tab tab, @WebContentsObserver.ViewportFitType int value) {
        if (tab == null || tab.isNativePage()) {
            return isNativeTabDrawingToBottomEdge(tab);
        }
        if (sAlwaysDrawWebEdgeToEdgeForTesting || tab.shouldEnableEmbeddedMediaExperience()) {
            return true;
        }
        return value == ViewportFit.COVER || value == ViewportFit.COVER_FORCED_BY_USER_AGENT;
    }

    /** Return whether there's any safe area constraint found for the given tab. */
    static boolean hasSafeAreaConstraintForTab(@Nullable Tab tab) {
        if (tab == null) return false;

        SafeAreaInsetsTracker safeAreaInsetsTracker =
                DisplayCutoutController.getSafeAreaInsetsTracker(tab);
        return safeAreaInsetsTracker != null && safeAreaInsetsTracker.hasSafeAreaConstraint();
    }

    /** Whether a native tab will be drawn to the bottom edge. */
    static boolean isNativeTabDrawingToBottomEdge(@Nullable Tab activeTab) {
        // TODO(crbug.com/339025702): Check if we are in tab switcher when activeTab is null.
        if (activeTab == null) return false;

        NativePage nativePage = activeTab.getNativePage();
        return nativePage != null && nativePage.supportsEdgeToEdgeOnBottom();
    }

    /** Whether a native tab will be drawn top edge to edge. */
    static boolean isNativeTabDrawingToTopEdge(@Nullable Tab activeTab) {
        if (activeTab == null) return false;

        NativePage nativePage = activeTab.getNativePage();
        return nativePage != null && nativePage.supportsEdgeToEdgeOnTop();
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

        var rootInsets = insetsSupplier.get();
        assert rootInsets != null;

        return hasTappableNavigationBarFromInsets(rootInsets);
    }

    /** Returns whether the given window's insets contains a tappable navigation bar. */
    public static boolean hasTappableNavigationBarFromInsets(WindowInsetsCompat insets) {
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
     * Returns whether the EdgeToEdge refactor (unifying top inset consumption in {@link
     * EdgeToEdgeController} instead of {@link TopInsetCoordinator}) is enabled. For top
     * edge-to-edge behavior beyond the refactor, check {@link #isTopEdgeToEdgeEnabled()}.
     */
    public static boolean isEdgeToEdgeRefactorEnabled() {
        if (Build.VERSION.SDK_INT < VERSION_CODES.R) {
            return false;
        }
        return ChromeFeatureList.sEdgeToEdgeTopInset.isEnabled();
    }

    /**
     * Returns whether the top edge-to-edge feature is enabled (beyond the {@link
     * EdgeToEdgeController} refactor). Implies {@link #isEdgeToEdgeRefactorEnabled()}.
     */
    public static boolean isTopEdgeToEdgeEnabled() {
        return isEdgeToEdgeRefactorEnabled()
                && ChromeFeatureList.sEdgeToEdgeTopInsetEnableTopEdgeToEdge.getValue();
    }

    /**
     * Returns whether top edge-to-edge (edgeless top inset) is supported for the given activity.
     * Top edge-to-edge is supported on phones and not on tablets or automotive devices.
     */
    public static boolean isEdgelessTopInsetSupported(@Nullable Activity activity) {
        if (activity == null || !isEdgeToEdgeRefactorEnabled()) {
            return false;
        }
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)
                && !DeviceInfo.isAutomotive();
    }

    /**
     * Returns whether the given Tab supports drawing top edge to edge.
     *
     * @param tab The Tab to check.
     * @return True if the tab is a native page that supports top edge to edge, false otherwise.
     */
    public static boolean tabSupportsTopEdgeToEdge(@Nullable Tab tab) {
        // TODO(crbug.com/498302496): Currently top edge-to-edge is only supported on native pages.
        // Support for web pages (e.g. viewport-fit=cover) will be added in future iterations and
        // will check isTopEdgeToEdgeEnabled().
        if (!isEdgeToEdgeRefactorEnabled() || tab == null) {
            return false;
        }

        if (tab.isNativePage()) {
            return isNativeTabDrawingToTopEdge(tab);
        }

        return false;
    }

    /**
     * Returns whether the given tab is a regular (non-incognito) New Tab Page.
     *
     * @param tab The Tab to check.
     * @return True if the tab is non-incognito and has an NTP URL.
     */
    public static boolean isRegularNtp(@Nullable Tab tab) {
        return tab != null
                && !tab.isIncognito()
                && tab.getUrl() != null
                && UrlUtilities.isNtpUrl(tab.getUrl());
    }
}
