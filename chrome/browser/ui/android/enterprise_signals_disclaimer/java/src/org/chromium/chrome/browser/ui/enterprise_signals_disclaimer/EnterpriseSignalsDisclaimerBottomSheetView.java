// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * The bottom sheet content view for the enterprise signals disclaimer. Extends {@link
 * EnterpriseSignalsDisclaimerView} and implements {@link BottomSheetContent}.
 */
@NullMarked
class EnterpriseSignalsDisclaimerBottomSheetView extends EnterpriseSignalsDisclaimerView
        implements BottomSheetContent {
    private @Nullable Runnable mOnDestroyedCallback;

    /**
     * Constructs an {@link EnterpriseSignalsDisclaimerBottomSheetView}.
     *
     * @param context The Android {@link Context}.
     */
    public EnterpriseSignalsDisclaimerBottomSheetView(Context context) {
        super(context);
    }

    public void setOnDestroyedCallback(Runnable callback) {
        mOnDestroyedCallback = callback;
    }

    // BottomSheetContent implementation:
    @Override
    public View getContentView() {
        return this;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return super.getScrollViewScrollY();
    }

    @Override
    public void destroy() {
        if (mOnDestroyedCallback != null) {
            mOnDestroyedCallback.run();
            mOnDestroyedCallback = null;
        }
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        // Half height is not supported.
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.enterprise_signals_disclaimer_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.enterprise_signals_disclaimer_sheet_closed;
    }
}
