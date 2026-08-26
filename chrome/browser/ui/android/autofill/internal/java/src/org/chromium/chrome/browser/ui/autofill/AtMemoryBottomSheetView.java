// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ViewFlipper;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ScreenId;
import org.chromium.chrome.browser.ui.autofill.internal.R;

/** View wrapper for the AtMemory bottom sheet. */
@NullMarked
public class AtMemoryBottomSheetView implements HomeProperties.SearchDelegate {
    private final View mContentView;
    private final AtMemoryHomeView mHomeView;
    private final AtMemoryFlyoutView mFlyoutView;
    // Observable supplier to notify the bottom sheet controller when the bottom sheet
    // can intercept back press events.
    private final SettableNonNullObservableSupplier<Boolean> mBackPressStateChangedSupplier;

    public AtMemoryBottomSheetView(Context context) {
        mContentView = LayoutInflater.from(context).inflate(R.layout.at_memory_bottom_sheet, null);

        mHomeView = mContentView.findViewById(R.id.at_memory_home_screen);
        mFlyoutView = mContentView.findViewById(R.id.at_memory_flyout_screen);

        mBackPressStateChangedSupplier = ObservableSuppliers.createNonNull(false);
    }

    public void setCurrentScreen(@ScreenId int screenId) {
        ViewFlipper viewFlipper = mContentView.findViewById(R.id.at_memory_view_flipper);
        viewFlipper.setDisplayedChild(getDisplayedChildForScreenId(screenId));
        mBackPressStateChangedSupplier.set(screenId == ScreenId.FLYOUT_SCREEN);
    }

    @ScreenId
    public int getCurrentScreen() {
        ViewFlipper viewFlipper = mContentView.findViewById(R.id.at_memory_view_flipper);
        return getScreenIdForDisplayedChild(viewFlipper.getDisplayedChild());
    }

    public View getContentView() {
        return mContentView;
    }

    public NonNullObservableSupplier<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    public AtMemoryHomeView getHomeView() {
        return mHomeView;
    }

    public AtMemoryFlyoutView getFlyoutView() {
        return mFlyoutView;
    }

    public void focusSearchArea() {
        mHomeView.focusSearchArea();
    }

    public void clearSearchText() {
        mHomeView.clearSearchText();
    }

    /**
     * Handles back navigation events, such as a back button press or edge swipe gesture, returning
     * from the flyout screen to the home screen.
     */
    public boolean onBackPressed() {
        if (getCurrentScreen() == ScreenId.FLYOUT_SCREEN) {
            mFlyoutView.onBackPressed();
            return true;
        }
        return false;
    }

    @Override
    public void hideKeyboardAndClearFocus() {
        mHomeView.hideKeyboardAndClearFocus();
    }

    public boolean searchHasFocus() {
        return mHomeView.searchHasFocus();
    }

    private int getDisplayedChildForScreenId(@ScreenId int screenId) {
        switch (screenId) {
            case ScreenId.HOME_SCREEN:
                return 0;
            case ScreenId.FLYOUT_SCREEN:
                return 1;
        }
        assert false : "Undefined ScreenId: " + screenId;
        return 0;
    }

    private @ScreenId int getScreenIdForDisplayedChild(int displayedChild) {
        switch (displayedChild) {
            case 0:
                return ScreenId.HOME_SCREEN;
            case 1:
                return ScreenId.FLYOUT_SCREEN;
        }
        assert false : "Undefined displayedChild: " + displayedChild;
        return ScreenId.HOME_SCREEN;
    }
}
