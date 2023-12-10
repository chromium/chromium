// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.accessibility.AccessibilityState.State;

public class DeviceAccessibilitySettingsHandler implements AccessibilityState.Listener {
    private static DeviceAccessibilitySettingsHandler sInstance;

    private final BrowserContextHandle mBrowserContextHandle;

    private DeviceAccessibilitySettingsHandler(BrowserContextHandle browserContextHandle) {
        mBrowserContextHandle = browserContextHandle;
    }

    public static DeviceAccessibilitySettingsHandler getInstance(
            BrowserContextHandle browserContextHandle) {
        if (sInstance == null) {
            sInstance = new DeviceAccessibilitySettingsHandler(browserContextHandle);
            AccessibilityState.addListener(sInstance);
        }
        return sInstance;
    }

    @Override
    public void onAccessibilityStateChanged(
            State oldAccessibilityState, State newAccessibilityState) {
        updateFontWeightAdjustment();
    }

    public void updateFontWeightAdjustment() {
        UserPrefs.get(mBrowserContextHandle)
                .setInteger(
                        "settings.a11y.font_weight_adjustment",
                        AccessibilityState.getFontWeightAdjustment());
    }
}
