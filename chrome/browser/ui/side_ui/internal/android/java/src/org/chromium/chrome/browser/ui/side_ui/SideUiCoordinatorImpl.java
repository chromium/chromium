// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Implementation of {@link SideUiCoordinator}. */
@NullMarked
final class SideUiCoordinatorImpl implements SideUiCoordinator {

    // TODO(crbug.com/478338737): Update to account for multiple side containers.
    @Nullable private SideUiContainer mSideUiContainer;
    private final ObserverList<SideUiObserver> mSideUiObservers = new ObserverList<>();

    @Override
    public void registerSideUiContainer(SideUiContainer sideUiContainer) {
        assert mSideUiContainer == null : "Registering a SideUiContainer when already set.";
        mSideUiContainer = sideUiContainer;
    }

    @Override
    public void unregisterSideUiContainer(SideUiContainer sideUiContainer) {
        assert mSideUiContainer == sideUiContainer : "Unregistering unknown SideUiContainer.";
        mSideUiContainer = null;
    }

    @Override
    public void addObserver(SideUiObserver observer) {
        if (mSideUiObservers.addObserver(observer)) {
            observer.onSideUiSpecsChanged(getSideUiSpecs());
        }
    }

    @Override
    public void removeObserver(SideUiObserver observer) {
        if (mSideUiObservers.removeObserver(observer)) {
            observer.onSideUiSpecsChanged(SideUiSpecs.EMPTY_SIDE_UI_SPECS);
        }
    }

    @Override
    public void requestUpdateContainer(SideUiContainerProperties properties) {
        // TODO(crbug.com/478306743): Implement.
    }

    @Override
    public void destroy() {
        if (mSideUiContainer != null) unregisterSideUiContainer(mSideUiContainer);
    }

    private SideUiSpecs getSideUiSpecs() {
        // TODO(crbug.com/478306743): Implement when we attach the container's view to either a
        //  start or end anchored ViewGroup based on the AnchorSide.
        return SideUiSpecs.EMPTY_SIDE_UI_SPECS;
    }
}
