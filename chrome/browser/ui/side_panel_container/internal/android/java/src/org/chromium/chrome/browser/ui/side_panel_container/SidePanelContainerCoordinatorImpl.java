// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import android.view.View;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;

/** Implementation of {@link SidePanelContainerCoordinator}. */
@NullMarked
final class SidePanelContainerCoordinatorImpl
        implements SidePanelContainerCoordinator, SideUiContainer {

    private final SideUiCoordinator mSideUiCoordinator;

    SidePanelContainerCoordinatorImpl(SideUiCoordinator sideUiCoordinator) {
        mSideUiCoordinator = sideUiCoordinator;
    }

    @Override
    public void init() {
        mSideUiCoordinator.registerSideUiContainer(this);
    }

    @Override
    public void populateContent(SidePanelContent content) {}

    @Override
    public void removeContent() {}

    @Override
    public void destroy() {
        mSideUiCoordinator.unregisterSideUiContainer(this);
    }

    @Override
    public View getView() {
        throw new UnsupportedOperationException();
    }

    @Override
    @Px
    public int determineContainerWidth(@Px int availableWidth, @Px int windowWidth) {
        throw new UnsupportedOperationException();
    }

    @Override
    @Px
    public int getCurrentWidth() {
        throw new UnsupportedOperationException();
    }

    @Override
    public void setWidth(int width) {
        throw new UnsupportedOperationException();
    }
}
