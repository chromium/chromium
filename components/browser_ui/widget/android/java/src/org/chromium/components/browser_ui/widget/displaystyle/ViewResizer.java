// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.displaystyle;

import android.view.View;

/**
 * Changes a view's padding when switching between {@link UiConfig} display styles. If the display
 * style is {@link HorizontalDisplayStyle#REGULAR}, a predetermined value will be used to set the
 * lateral padding. If the display style is {@link HorizontalDisplayStyle#WIDE}, the lateral padding
 * will be calculated using the available screen width to keep the view constrained to {@link
 * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}
 */
public class ViewResizer implements DisplayStyleObserver, View.OnLayoutChangeListener {
    /** The default value for the lateral padding. */
    private int mDefaultPaddingPixels;

    /** The minimum wide display value used for the lateral padding. */
    private int mMinWidePaddingPixels;

    private final View mView;
    private final DisplayStyleObserverAdapter mDisplayStyleObserver;
    protected final UiConfig mUiConfig;

    /**
     * @param view The view that will have its padding resized.
     * @param config The UiConfig object to subscribe to.
     * @param defaultPaddingPixels Padding to use in {@link HorizontalDisplayStyle#REGULAR}.
     * @param minWidePaddingPixels Minimum lateral padding to use in {@link
     *         HorizontalDisplayStyle#WIDE}.
     */
    public ViewResizer(
            View view, UiConfig config, int defaultPaddingPixels, int minWidePaddingPixels) {
        mView = view;
        mDefaultPaddingPixels = defaultPaddingPixels;
        mMinWidePaddingPixels = minWidePaddingPixels;
        mUiConfig = config;
        mDisplayStyleObserver = new DisplayStyleObserverAdapter(view, config, this);
        view.addOnLayoutChangeListener(this);
    }

    /**
     * Convenience method to create a new ViewResizer and immediately attach it to a {@link
     * UiConfig}. If the {@link UiConfig} can outlive the view, the regular constructor should be
     * used, so it can be detached to avoid memory leaks.
     * @param view The view that will have its padding resized.
     * @param config The UiConfig object to subscribe to.
     * @param defaultPaddingPixels Padding to use in {@link HorizontalDisplayStyle#REGULAR}.
     * @param minWidePaddingPixels Minimum lateral padding to use in {@link
     *         HorizontalDisplayStyle#WIDE}.
     * @return The {@link ViewResizer} that is created and attached.
     */
    public static ViewResizer createAndAttach(
            View view, UiConfig config, int defaultPaddingPixels, int minWidePaddingPixels) {
        ViewResizer viewResizer =
                new ViewResizer(view, config, defaultPaddingPixels, minWidePaddingPixels);
        viewResizer.attach();
        return viewResizer;
    }

    /** Attaches to the {@link UiConfig}. */
    public void attach() {
        mDisplayStyleObserver.attach();
    }

    /** Detaches from the {@link UiConfig}. */
    public void detach() {
        mDisplayStyleObserver.detach();
    }

    @Override
    public void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
        updatePadding();
    }

    /**
     * Sets the lateral padding on the associated view, using the appropriate value depending on
     * the current display style.
     * @param defaultPaddingPixels Padding to use in {@link HorizontalDisplayStyle#REGULAR}.
     * @param minWidePaddingPixels Minimum lateral padding to use in {@link
     *         HorizontalDisplayStyle#WIDE}.
     */
    public void setPadding(int defaultPaddingPixels, int minWidePaddingPixels) {
        mDefaultPaddingPixels = defaultPaddingPixels;
        mMinWidePaddingPixels = minWidePaddingPixels;
        updatePadding();
    }

    private void updatePadding() {
        int padding = computePadding();
        mView.setPaddingRelative(padding, mView.getPaddingTop(), padding, mView.getPaddingBottom());
    }

    /** Computes the lateral padding to be applied to the associated view. */
    protected int computePadding() {
        if (!mUiConfig.getCurrentDisplayStyle().isWide()) return mDefaultPaddingPixels;
        return ViewResizerUtil.computePaddingForWideDisplay(
                mUiConfig.getContext(), mView, mMinWidePaddingPixels);
    }

    protected int getMinWidePaddingPixels() {
        return mMinWidePaddingPixels;
    }

    @Override
    public void onLayoutChange(
            View view, int i, int i1, int i2, int i3, int i4, int i5, int i6, int i7) {
        updatePadding();
    }
}
