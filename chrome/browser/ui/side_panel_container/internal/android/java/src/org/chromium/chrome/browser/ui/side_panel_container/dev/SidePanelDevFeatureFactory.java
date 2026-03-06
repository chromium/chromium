// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.dev;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;

/** Factory for creating a {@link SidePanelDevFeature}. */
@NullMarked
public final class SidePanelDevFeatureFactory {
    private SidePanelDevFeatureFactory() {}

    @Nullable
    public static SidePanelDevFeature create(
            Activity parentActivity, SidePanelContainerCoordinator sidePanelContainerCoordinator) {
        if (!ChromeFeatureList.sEnableAndroidSidePanelDevFeature.isEnabled()) {
            return null;
        }

        return new SidePanelDevFeatureImpl(parentActivity, sidePanelContainerCoordinator);
    }
}
