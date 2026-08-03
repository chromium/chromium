// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.app.Activity;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;

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
     * @param layoutStateProviderSupplier Supplier for the {@link LayoutStateProvider}.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} to adjust for
     *     top controls changes.
     * @param fullscreenManager The {@link FullscreenManager} for observing tab fullscreen mode.
     * @param topControlsStacker The {@link TopControlsStacker} to calculate heights for top
     *     controls.
     * @param anchorContainerParent The {@link ViewGroup} that is the parent for the side UI
     *     containers.
     * @param leftAnchorContainerStub The {@link ViewStub} for the left-anchored container.
     * @param rightAnchorContainerStub The {@link ViewStub} for the right-anchored container.
     * @param webContentHairlineContainerStub The {@link ViewStub} for the web content hairline
     *     container.
     * @param tabStripBottomPxSupplier The supplier for the Side UI's top margin added for tab
     *     strip.
     * @param incognitoStateProvider The {@link IncognitoStateProvider} to observe incognito state.
     * @return The newly-created {@link SideUiCoordinator}, or {@code null} if it was not created.
     */
    @Nullable
    public static SideUiCoordinator create(
            Activity parentActivity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            FullscreenManager fullscreenManager,
            TopControlsStacker topControlsStacker,
            @Nullable ViewGroup anchorContainerParent,
            @Nullable ViewStub leftAnchorContainerStub,
            @Nullable ViewStub rightAnchorContainerStub,
            @Nullable ViewStub webContentHairlineContainerStub,
            @Nullable NonNullObservableSupplier<Integer> tabStripBottomPxSupplier,
            IncognitoStateProvider incognitoStateProvider) {
        if (!AndroidSidePanelEnabledFn.isEnabled()
                && !VerticalTabUtils.isVerticalTabsEligible(parentActivity)) {
            return null;
        }

        assert anchorContainerParent != null;
        assert leftAnchorContainerStub != null;
        assert rightAnchorContainerStub != null;
        assert webContentHairlineContainerStub != null;

        if (tabStripBottomPxSupplier == null) {
            tabStripBottomPxSupplier = ObservableSuppliers.createNonNull(0);
        }
        return new SideUiCoordinatorImpl(
                parentActivity,
                lifecycleDispatcher,
                layoutStateProviderSupplier,
                browserControlsStateProvider,
                fullscreenManager,
                topControlsStacker,
                anchorContainerParent,
                leftAnchorContainerStub,
                rightAnchorContainerStub,
                webContentHairlineContainerStub,
                tabStripBottomPxSupplier,
                incognitoStateProvider);
    }
}
