// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.NonNull;
import androidx.core.view.AccessibilityDelegateCompat;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;

/**
 * An AccessibilityDelegate for AccountSelectionBottomSheetContent that disables the half sheet
 * state when a view gains accessibility focus.
 */
public class ExpandOnFocusAccessibilityDelegate extends AccessibilityDelegateCompat {

    private final AccountSelectionBottomSheetContent mBottomSheetContent;
    private final BottomSheetController mBottomSheetController;

    public ExpandOnFocusAccessibilityDelegate(
            @NonNull AccountSelectionBottomSheetContent bottomSheetContent,
            @NonNull BottomSheetController bottomSheetController) {
        mBottomSheetContent = bottomSheetContent;
        mBottomSheetController = bottomSheetController;
    }

    @Override
    public boolean onRequestSendAccessibilityEvent(
            @NonNull ViewGroup host, @NonNull View child, @NonNull AccessibilityEvent event) {
        // Check if the event type is a view gaining accessibility focus
        if (event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED) {
            if (mBottomSheetController.getSheetState() != SheetState.FULL) {
                mBottomSheetContent.setIsPassiveModeHalfHeightEnabled(false);
                mBottomSheetController.expandSheet();
            }
        }
        return super.onRequestSendAccessibilityEvent(host, child, event);
    }
}
