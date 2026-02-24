// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.view.ViewStub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Factory for creating a {@link SideUiCoordinator}. */
@NullMarked
public final class SideUiCoordinatorFactory {
    private SideUiCoordinatorFactory() {}

    /**
     * Creates a {@link SideUiCoordinator}.
     *
     * @param startAnchorContainerStub The {@link ViewStub} for the start-anchored container.
     * @param endAnchorContainerStub The {@link ViewStub} for the end-anchored container.
     * @return The newly-created {@link SideUiCoordinator}, or {@code null} if it was not created.
     */
    @Nullable
    public static SideUiCoordinator create(
            @Nullable ViewStub startAnchorContainerStub,
            @Nullable ViewStub endAnchorContainerStub) {
        if (!ChromeFeatureList.sEnableAndroidSidePanel.isEnabled()) {
            return null;
        }

        assert startAnchorContainerStub != null;
        assert endAnchorContainerStub != null;

        return new SideUiCoordinatorImpl(startAnchorContainerStub, endAnchorContainerStub);
    }
}
