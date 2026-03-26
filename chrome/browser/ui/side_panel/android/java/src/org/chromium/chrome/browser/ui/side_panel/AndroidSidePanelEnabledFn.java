// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Checks all conditions for whether to enable the Android Side Panel.
 *
 * <p>This is the sole authority that decides whether to enable Android Side Panel.
 *
 * <p>TODO(crbug.com/496609444): Replace all flag checks with this class.
 */
@NullMarked
public final class AndroidSidePanelEnabledFn {
    private AndroidSidePanelEnabledFn() {}

    /** Returns true if the Android Side Panel should be enabled. */
    @CalledByNative
    public static boolean isEnabled() {
        return ChromeFeatureList.sEnableAndroidSidePanel.isEnabled();
    }
}
