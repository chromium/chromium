// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.dev;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeatureKey;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.function.Supplier;

/** Factory for creating a {@link SidePanelDevFeature}. */
@NullMarked
public final class SidePanelDevFeatureFactory {

    private SidePanelDevFeatureFactory() {}

    @Nullable
    public static SidePanelDevFeature create(
            ChromeAndroidTask chromeAndroidTask,
            Profile profile,
            ActivityWindowAndroid windowAndroid,
            Supplier<Tab> tabSupplier) {
        if (AndroidSidePanelEnabledFn.isWindowScopedDevFeatureEnabled()) {
            return (SidePanelWindowScopedDevFeatureImpl)
                    chromeAndroidTask.addFeature(
                            new ChromeAndroidTaskFeatureKey(
                                    SidePanelWindowScopedDevFeatureImpl.class,
                                    profile,
                                    windowAndroid),
                            () -> new SidePanelWindowScopedDevFeatureImpl(profile, windowAndroid));
        }

        if (AndroidSidePanelEnabledFn.isTabScopedDevFeatureEnabled()) {
            return new SidePanelTabScopedDevFeatureImpl(tabSupplier);
        }

        return null;
    }
}
