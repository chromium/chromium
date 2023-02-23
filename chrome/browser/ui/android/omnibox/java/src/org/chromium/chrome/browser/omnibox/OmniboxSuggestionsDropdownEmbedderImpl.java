// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.View;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.WindowInsets;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowDelegate;

/**
 * Implementation of {@link OmniboxSuggestionsDropdownEmbedder} that positions it using an "anchor"
 * and "horizontal alignment" view. It aligns the omnibox dropdown as follows:
 * - The top of the dropdown is the bottom of the anchor view.
 * - The width of the dropdown is equal to the anchor view's width.
 * - The horizontal padding of the dropdown is calculated so that its contents are in line with the
 *   horizontal alignment view if the alignment view's width is smaller than the anchor view.
 * */
class OmniboxSuggestionsDropdownEmbedderImpl implements OmniboxSuggestionsDropdownEmbedder,
                                                        View.OnLayoutChangeListener,
                                                        OnGlobalLayoutListener {
    private final ObservableSupplierImpl<OmniboxAlignment> mOmniboxAlignmentSupplier =
            new ObservableSupplierImpl<>();
    private final @NonNull WindowAndroid mWindowAndroid;
    private final @NonNull WindowDelegate mWindowDelegate;
    private final @NonNull View mAnchorView;
    private final @NonNull View mHorizontalAlignmentView;
    // Reusable int array to pass to positioning methods that operate on a two element int array.
    // Keeping it as a member lets us avoid allocating a temp array every time.
    private final int[] mPositionArray = new int[2];
    private int mVerticalOffsetInWindow;
    private WindowInsetsCompat mWindowInsetsCompat;

    /**
     *
     * @param windowAndroid Window object in which the dropdown will be displayed.
     * @param windowDelegate Delegate object for performing window operations.
     * @param anchorView View to which the dropdown should be "anchored" i.e. vertically positioned
     *         next to and matching the width of. This must be a descendant of the top-level content
     *         (android.R.id.content) view.
     * @param horizontalAlignmentView View to which the dropdown should be horizontally aligned when
     *         its width is smaller than the anchor view. This must be a descendant of the anchor
     *         view.
     */
    OmniboxSuggestionsDropdownEmbedderImpl(@NonNull WindowAndroid windowAndroid,
            @NonNull WindowDelegate windowDelegate, @NonNull View anchorView,
            @NonNull View horizontalAlignmentView) {
        mWindowAndroid = windowAndroid;
        mWindowDelegate = windowDelegate;
        mAnchorView = anchorView;
        mHorizontalAlignmentView = horizontalAlignmentView;
        recalculateOmniboxAlignment();
    }

    @Override
    public OmniboxAlignment addAlignmentObserver(Callback<OmniboxAlignment> obs) {
        return mOmniboxAlignmentSupplier.addObserver(obs);
    }

    @Override
    public void removeAlignmentObserver(Callback<OmniboxAlignment> obs) {
        mOmniboxAlignmentSupplier.removeObserver(obs);
    }

    @Nullable
    @Override
    public OmniboxAlignment getCurrentAlignment() {
        return mOmniboxAlignmentSupplier.get();
    }

    @Override
    public boolean isTablet() {
        return DeviceFormFactor.isWindowOnTablet(mWindowAndroid);
    }

    @Override
    public void onAttachedToWindow() {
        mAnchorView.addOnLayoutChangeListener(this);
        mHorizontalAlignmentView.addOnLayoutChangeListener(this);
        mAnchorView.getViewTreeObserver().addOnGlobalLayoutListener(this);
        recalculateOmniboxAlignment();
    }

    @Override
    public void onDetachedFromWindow() {
        mAnchorView.removeOnLayoutChangeListener(this);
        mHorizontalAlignmentView.removeOnLayoutChangeListener(this);
        mAnchorView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
    }

    @Override
    @NonNull
    public WindowDelegate getWindowDelegate() {
        return mWindowDelegate;
    }

    // View.OnLayoutChangeListener
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        recalculateOmniboxAlignment();
    }

    // OnGlobalLayoutListener
    @Override
    public void onGlobalLayout() {
        if (offsetInWindowChanged(mAnchorView) || insetsHaveChanged(mAnchorView)) {
            recalculateOmniboxAlignment();
        }
    }

    /**
     * Recalculates the desired alignment of the omnibox and sends the updated alignment data to
     * any observers. Currently will send an update message unconditionally. This method is called
     * during layout and should avoid memory allocations other than the necessary new
     * OmniboxAlignment().
     */
    void recalculateOmniboxAlignment() {
        int left = 0;
        View contentView = mAnchorView.getRootView().findViewById(android.R.id.content);
        ViewUtils.getRelativeLayoutPosition(contentView, mAnchorView, mPositionArray);
        int top = mPositionArray[1] + mAnchorView.getMeasuredHeight();
        int width = mAnchorView.getMeasuredWidth();
        int paddingLeft;
        int paddingRight;
        if (isTablet()) {
            ViewUtils.getRelativeLayoutPosition(
                    mAnchorView, mHorizontalAlignmentView, mPositionArray);
            paddingLeft = mPositionArray[0];
            paddingRight = mAnchorView.getMeasuredWidth()
                    - mHorizontalAlignmentView.getMeasuredWidth() - mPositionArray[0];
        } else {
            paddingLeft = 0;
            paddingRight = 0;
        }

        // TODO(pnoland@, https://crbug.com/1416985): calculate height as well and avoid pushing
        // changes that are identical to the previous alignment value.
        OmniboxAlignment omniboxAlignment =
                new OmniboxAlignment(left, top, width, paddingLeft, paddingRight);
        mOmniboxAlignmentSupplier.set(omniboxAlignment);
    }

    /**
     * Returns whether the given view's position in the window has changed since the last call to
     * offsetInWindowChanged().
     */
    private boolean offsetInWindowChanged(View view) {
        view.getLocationInWindow(mPositionArray);
        boolean result = mVerticalOffsetInWindow != mPositionArray[1];
        mVerticalOffsetInWindow = mPositionArray[1];
        return result;
    }

    /**
     * Returns whether the window insets corresponding to the given view have changed since the
     * last call to insetsHaveChanged().
     */
    private boolean insetsHaveChanged(View view) {
        WindowInsets rootWindowInsets = view.getRootWindowInsets();
        if (rootWindowInsets == null) return false;
        WindowInsetsCompat windowInsetsCompat =
                WindowInsetsCompat.toWindowInsetsCompat(view.getRootWindowInsets(), view);
        boolean result = !windowInsetsCompat.equals(mWindowInsetsCompat);
        mWindowInsetsCompat = windowInsetsCompat;
        return result;
    }
}
