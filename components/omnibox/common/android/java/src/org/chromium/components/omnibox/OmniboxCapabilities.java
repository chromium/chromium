// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.content.Context;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.DeviceInput;

/** This class provides information about device and OS capabilities relevant to Omnibox. */
@NullMarked
public class OmniboxCapabilities {
    // Threshold for low RAM devices. We won't be showing suggestion images
    // on devices that have less RAM than this to avoid bloat and reduce user-visible
    // slowdown while spinning up an image decompression process.
    // We set the threshold to 1.5GB to reduce number of users affected by this restriction.
    private static final int LOW_MEMORY_THRESHOLD_KB = (int) (1.5 * 1024 * 1024);

    /// Holds the information whether logic should focus on preserving memory on this device.
    private static @Nullable Boolean sIsLowMemoryDevice;

    /** See {@link #setHasDesktopExperienceForTesting(Boolean)}. */
    private static @Nullable Boolean sHasDesktopExperienceForTesting;

    /** See {@link #setIsDesktopPlatformForTesting(Boolean)}. */
    private static @Nullable Boolean sIsDesktopPlatformForTesting;

    /**
     * Returns whether the omnibox's recycler view pool should be pre-warmed prior to initial use.
     */
    public static boolean shouldPreWarmRecyclerViewPool() {
        return !isLowMemoryDevice();
    }

    /**
     * Returns whether the device is to be considered low-end for any memory intensive operations.
     */
    public static boolean isLowMemoryDevice() {
        if (sIsLowMemoryDevice == null) {
            sIsLowMemoryDevice =
                    (SysUtils.amountOfPhysicalMemoryKB() < LOW_MEMORY_THRESHOLD_KB
                            && !CommandLine.getInstance()
                                    .hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE));
        }
        return sIsLowMemoryDevice;
    }

    /** Indicate a low memory device for testing purposes. */
    public static void setIsLowMemoryDeviceForTesting(boolean isLowMemDevice) {
        sIsLowMemoryDevice = isLowMemDevice;
        ResettersForTesting.register(() -> sIsLowMemoryDevice = null);
    }

    /** Returns whether the device type is supported for Fusebox. */
    public static boolean isFuseboxSupportedDeviceType() {
        return !DeviceInfo.isAutomotive() && !DeviceInfo.isXr() && !DeviceInfo.isTV();
    }

    /**
     * Return whether the device is in a desktop-like configuration (interacted with using physical
     * keyboard and precision pointer).
     *
     * <p>We're not limiting to tablet modes here, because narrow windows on LFF devices are
     * eligible for Desktop treatment, too.
     *
     * @param context the context to use to determine device form factor
     */
    public static boolean hasDesktopExperience(Context context) {
        if (sHasDesktopExperienceForTesting != null) {
            return sHasDesktopExperienceForTesting;
        }

        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && DeviceInput.supportsAlphabeticKeyboard()
                && DeviceInput.supportsPrecisionPointer();
    }

    /** Modifies the output of {@link #hasDesktopExperience(Context)} for testing. */
    public static void setHasDesktopExperienceForTesting(Boolean hasDesktopExperience) {
        sHasDesktopExperienceForTesting = hasDesktopExperience;
        ResettersForTesting.register(() -> sHasDesktopExperienceForTesting = null);
    }

    /**
     * Return whether the current platform is specifically a desktop platform.
     *
     * <p>This call should be used sparingly - only to gate features that are strictly Desktop
     * specific. All other calls should defer to {@link #hasDesktopExperience(Context)}.
     */
    public static boolean isDesktopPlatform() {
        if (sIsDesktopPlatformForTesting != null) {
            return sIsDesktopPlatformForTesting;
        }
        return DeviceInfo.isDesktop();
    }

    /** Modifies the output of {@link #isDesktopPlatform()} for testing. */
    public static void setIsDesktopPlatformForTesting(Boolean isDesktopPlatform) {
        sIsDesktopPlatformForTesting = isDesktopPlatform;
        ResettersForTesting.register(() -> sIsDesktopPlatformForTesting = null);
    }
}
