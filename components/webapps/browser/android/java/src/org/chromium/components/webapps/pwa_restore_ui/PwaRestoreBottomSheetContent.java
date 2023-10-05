// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.webapps.R;

/**
 * The class handling the bottom sheet install for the PWA Restore UI.
 */
public class PwaRestoreBottomSheetContent implements BottomSheetContent {
    // The view for our bottom sheet.
    private final PwaRestoreBottomSheetView mView;

    // This content's priority.
    private @ContentPriority int mPriority = ContentPriority.LOW;

    public PwaRestoreBottomSheetContent(PwaRestoreBottomSheetView view) {
        mView = view;
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
        return mView.getPreviewView();
    }

    @Override
    public float getFullHeightRatio() {
        return 1f;
    }

    @Override
    public float getHalfHeightRatio() {
        // By default `expandSheet` will result in the sheet expanding only to half-full height.
        // This disables that functionality so we can go straight to full screen (minus the top
        // system toolbar).
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return mPriority;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.pwa_restore_bottom_sheet_accessibility;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.pwa_restore_bottom_sheet_accessibility;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.pwa_restore_bottom_sheet_accessibility;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.pwa_restore_bottom_sheet_accessibility;
    }
}
