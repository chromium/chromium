// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.bottomsheet;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.webapps.AddToHomescreenViewDelegate;
import org.chromium.components.webapps.R;

/**
 * The class handling the bottom sheet install for PWA installs. The UI is shown on construction
 * using the supplied BottomSheetController.
 */
public class PwaInstallBottomSheetContent implements BottomSheetContent {
    /** The view for our bottom sheet. */
    private final PwaInstallBottomSheetView mView;

    /** The delegate handling the install. */
    @VisibleForTesting protected final AddToHomescreenViewDelegate mDelegate;

    /** This content's priority. */
    private @ContentPriority int mPriority = ContentPriority.LOW;

    public PwaInstallBottomSheetContent(
            PwaInstallBottomSheetView view, AddToHomescreenViewDelegate delegate) {
        mView = view;
        mDelegate = delegate;
    }

    public void setPriority(@ContentPriority int priority) {
        mPriority = priority;
    }

    // BottomSheetContent:

    @Override
    public View getContentView() {
        return mView.getContentView();
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getPeekHeight() {
        return mView.getPeekHeight();
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
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
        return mPriority;
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
