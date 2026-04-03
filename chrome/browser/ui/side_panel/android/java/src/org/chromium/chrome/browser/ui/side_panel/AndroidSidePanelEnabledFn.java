// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.jni_zero.CalledByNative;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Checks all conditions for whether to enable the Android Side Panel.
 *
 * <p>This is the sole authority that decides whether to enable Android Side Panel.
 */
@NullMarked
public final class AndroidSidePanelEnabledFn {
    private AndroidSidePanelEnabledFn() {}

    /** Returns true if the Android Side Panel should be enabled. */
    @CalledByNative
    public static boolean isEnabled() {

        // In http://crrev.com/c/7689838, the cached feature flag's "defaultValueInTests" was set to
        // true for reasons in that CL's commit message.
        //
        // However, that caused the flag to be also enabled in non-official or non-Chrome-branded
        // browser app APKs on _mobile_ Android where there should be no side panel
        // (see CachedFlag.java for details).
        //
        // To temporarily fix mobile Android, we only read the cached feature flag when the
        // "IS_DESKTOP_ANDROID" build flag is true, but this isn't 100% correct: side panel
        // shouldn't be restricted to desktop Android; it should be available to all large form
        // factors.
        //
        // Note that we can't use "IS_DESKTOP_ANDROID" to set the cached feature flag's
        // "defaultValueInTests" as that will require fieldtrial_testing_config.json to be
        // build-flag-dependent. As we only have one fieldtrial_testing_config.json, making
        // "defaultValueInTests = IS_DESKTOP_ANDROID" won't make
        // ChromeCachedFlagsTest#testValueIsConsistentWithDefault pass on all bots.
        //
        // TODO(crbug.com/499090354): Properly enable the feature flag for all large form factors.
        if (BuildConfig.IS_DESKTOP_ANDROID) {
            // TODO(crbug.com/497862593): See if the cached flag can be instantiated inside this
            // class.
            return ChromeFeatureList.sEnableAndroidSidePanel.isEnabled();
        }

        return false;
    }
}
