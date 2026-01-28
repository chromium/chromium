// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Factory for creating a {@link SideUiCoordinator}. */
@NullMarked
public final class SideUiCoordinatorFactory {
    private SideUiCoordinatorFactory() {}

    @Nullable
    public static SideUiCoordinator create() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)) {
            return null;
        }

        return new SideUiCoordinatorImpl();
    }
}
