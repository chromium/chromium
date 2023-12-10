// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import androidx.annotation.Nullable;

import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.util.AccessibilityUtil;

/** Provides the chrome specific wiring for AccessibilityUtil. */
public class ChromeAccessibilityUtil extends AccessibilityUtil {
    private static ChromeAccessibilityUtil sInstance;

    public static ChromeAccessibilityUtil get() {
        if (sInstance == null) {
            sInstance = new ChromeAccessibilityUtil();
            AccessibilityState.addListener(sInstance);
        }
        return sInstance;
    }

    private ChromeAccessibilityUtil() {}

    @Deprecated
    public boolean isAccessibilityEnabled() {
        return AccessibilityState.isAccessibilityEnabled();
    }

    @Override
    public void setAccessibilityEnabledForTesting(@Nullable Boolean isEnabled) {
        AccessibilityState.setIsPerformGesturesEnabledForTesting(Boolean.TRUE.equals(isEnabled));
        AccessibilityState.setIsTouchExplorationEnabledForTesting(Boolean.TRUE.equals(isEnabled));
        super.setAccessibilityEnabledForTesting(isEnabled);
    }
}
