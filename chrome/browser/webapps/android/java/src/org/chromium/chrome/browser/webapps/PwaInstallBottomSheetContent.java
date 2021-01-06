// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * The class handling the bottom sheet install for PWA installs. The UI is shown on construction
 * using the supplied BottomSheetController.
 */
public class PwaInstallBottomSheetContent implements BottomSheetContent {
    /** The view for our bottom sheet. */
    private final PwaInstallBottomSheetView mView;

    /** The delegate handling the install. */
    @VisibleForTesting
    protected final AddToHomescreenViewDelegate mDelegate;

    public PwaInstallBottomSheetContent(
            PwaInstallBottomSheetView view, AddToHomescreenViewDelegate delegate) {
        mView = view;
        mDelegate = delegate;
    }

    // BottomSheetContent:

    @Override
    public View getContentView() {
        return mView.getContentView();
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return mView.getToolbarView();
    }

    @Override
    public int getVerticalScrollOffset() {
        // TODO(finnur): Handle this correctly for small screens.
        return 0;
    }

    @Override
    public void destroy() {
        mDelegate.onViewDismissed();
    }

    @Override
    public int getPriority() {
        // The bottom sheet is the result of a user action, either when triggered by the
        // 'Install app' command in the App Menu or when navigating to a PWA. Hence HIGH priority.
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.pwa_install_bottom_sheet_accessibility;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.pwa_install_bottom_sheet_accessibility;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.pwa_install_bottom_sheet_accessibility;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.pwa_install_bottom_sheet_accessibility;
    }
}
