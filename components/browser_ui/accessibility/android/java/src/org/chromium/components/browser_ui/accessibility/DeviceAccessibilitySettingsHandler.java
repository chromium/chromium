// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.accessibility.AccessibilityState.State;

@NullMarked
public class DeviceAccessibilitySettingsHandler
        implements AccessibilityState.Listener, Destroyable {
    private final PrefService mPrefService;

    public DeviceAccessibilitySettingsHandler(PrefService prefService) {
        mPrefService = prefService;
        AccessibilityState.addListener(this);
    }

    @Override
    public void destroy() {
        AccessibilityState.removeListener(this);
    }

    @Override
    public void onAccessibilityStateChanged(
            State oldAccessibilityState, State newAccessibilityState) {
        updateFontWeightAdjustment();
    }

    public void updateFontWeightAdjustment() {
        mPrefService.setInteger(
                "settings.a11y.font_weight_adjustment",
                AccessibilityState.getFontWeightAdjustment());
    }
}
