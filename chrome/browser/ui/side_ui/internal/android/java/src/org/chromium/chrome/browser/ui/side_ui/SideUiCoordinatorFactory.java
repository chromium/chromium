// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.app.Activity;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;

/** Factory for creating a {@link SideUiCoordinator}. */
@NullMarked
public final class SideUiCoordinatorFactory {
    private SideUiCoordinatorFactory() {}

    /**
     * Creates a {@link SideUiCoordinator}.
     *
     * @param parentActivity The {@link Activity} containing all Side UIs.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for {@code
     *     parentActivity}.
     * @param anchorContainerParent The {@link ViewGroup} that is the parent for the side UI
     *     containers.
     * @param startAnchorContainerStub The {@link ViewStub} for the start-anchored container.
     * @param endAnchorContainerStub The {@link ViewStub} for the end-anchored container.
     * @param topMarginSupplier The supplier for the Side UI's top margin.
     * @return The newly-created {@link SideUiCoordinator}, or {@code null} if it was not created.
     */
    @Nullable
    public static SideUiCoordinator create(
            Activity parentActivity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            @Nullable ViewGroup anchorContainerParent,
            @Nullable ViewStub startAnchorContainerStub,
            @Nullable ViewStub endAnchorContainerStub,
            @Nullable NonNullObservableSupplier<Integer> topMarginSupplier) {
        if (!AndroidSidePanelEnabledFn.isEnabled()) {
            return null;
        }

        assert anchorContainerParent != null;
        assert startAnchorContainerStub != null;
        assert endAnchorContainerStub != null;

        if (topMarginSupplier == null) {
            topMarginSupplier = ObservableSuppliers.createNonNull(0);
        }
        return new SideUiCoordinatorImpl(
                parentActivity,
                lifecycleDispatcher,
                anchorContainerParent,
                startAnchorContainerStub,
                endAnchorContainerStub,
                topMarginSupplier);
    }
}
