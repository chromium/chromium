// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;

import androidx.appcompat.widget.Toolbar;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;
import java.util.List;

/**
 * Toolbar for the PDF viewer. To handle clicks on the navigation icon, set a listener with {@link
 * #setNavigationOnClickListener(OnClickListener)}.
 */
@NullMarked
public class PdfToolbar extends Toolbar implements View.OnLayoutChangeListener {
    /** Listener for width changes of the toolbar. */
    public interface OnWidthChangedListener {
        void onWidthChanged(int widthPx);
    }

    private @Nullable OnWidthChangedListener mOnWidthChangedListener;

    public void setOnWidthChangedListener(@Nullable OnWidthChangedListener listener) {
        mOnWidthChangedListener = listener;
    }
    private @Nullable View mDownloadButton;
    private @Nullable View mRotateButton;
    private @Nullable View mFitToPageButton;
    private @Nullable List<View> mZoomControls;
    private @Nullable List<View> mPageNav;
    private @Nullable View mEditButton;

    private @Nullable View mPageZoomDivider;
    private @Nullable View mZoomFitDivider;
    private @Nullable View mRotateEditDivider;

    private @Nullable ConstraintLayout mConstraintLayout;
    private @Nullable View mCenterGroup;
    private @Nullable View mEndGroup;
    private @Nullable View mTitle;

    private @Nullable Boolean mIsTitleConstrainedToCenter;

    public PdfToolbar(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        EditText currentPage = findViewById(R.id.current_page);
        currentPage.setFocusableInTouchMode(true);

        mDownloadButton = findViewById(R.id.download_button);
        mRotateButton = findViewById(R.id.rotate_button);
        mFitToPageButton = findViewById(R.id.fit_to_page_button);

        mZoomControls =
                Arrays.asList(
                        findViewById(R.id.zoom_decrease_button),
                        findViewById(R.id.zoom_value),
                        findViewById(R.id.zoom_increase_button));

        mPageNav =
                Arrays.asList(
                        currentPage,
                        findViewById(R.id.page_count_divider),
                        findViewById(R.id.page_count));

        mEditButton = findViewById(R.id.edit_button);

        mPageZoomDivider = findViewById(R.id.page_zoom_divider);
        mZoomFitDivider = findViewById(R.id.zoom_fit_divider);
        mRotateEditDivider = findViewById(R.id.rotate_edit_divider);

        mConstraintLayout = findViewById(R.id.pdf_toolbar_layout);
        mCenterGroup = findViewById(R.id.pdf_toolbar_group_center);
        mEndGroup = findViewById(R.id.pdf_toolbar_group_end);
        mTitle = findViewById(R.id.pdf_title);

        addOnLayoutChangeListener(this);
    }

    @Override
    public void onLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        int width = right - left;
        if (width != (oldRight - oldLeft)) {
            if (mOnWidthChangedListener != null) {
                mOnWidthChangedListener.onWidthChanged(width);
            }
        }
    }

    void setDownloadButtonVisible(boolean visible) {
        setViewVisibility(mDownloadButton, visible);
        updateDividersAndConstraints();
    }

    void setRotateButtonVisible(boolean visible) {
        setViewVisibility(mRotateButton, visible);
        updateDividersAndConstraints();
    }

    void setFitToPageButtonVisible(boolean visible) {
        setViewVisibility(mFitToPageButton, visible);
        updateDividersAndConstraints();
    }

    void setZoomControlsVisible(boolean visible) {
        setViewsVisibility(mZoomControls, visible);
        updateDividersAndConstraints();
    }

    void setPageNavAndEditVisible(boolean visible) {
        setViewsVisibility(mPageNav, visible);
        setViewVisibility(mEditButton, visible);
        setViewVisibility(mCenterGroup, visible);
        updateDividersAndConstraints();
    }

    private void updateDividersAndConstraints() {
        boolean showNavEdit = mEditButton != null && mEditButton.getVisibility() == View.VISIBLE;
        boolean showZoom =
                mZoomControls != null
                        && !mZoomControls.isEmpty()
                        && mZoomControls.get(0) != null
                        && mZoomControls.get(0).getVisibility() == View.VISIBLE;
        boolean showRotate = mRotateButton != null && mRotateButton.getVisibility() == View.VISIBLE;
        boolean showFit =
                mFitToPageButton != null && mFitToPageButton.getVisibility() == View.VISIBLE;

        // Dividers
        setViewVisibility(mPageZoomDivider, showNavEdit && showZoom);
        setViewVisibility(mZoomFitDivider, showZoom && showFit);
        setViewVisibility(mRotateEditDivider, showRotate && showNavEdit);

        // Adjust title constraints
        if (mConstraintLayout != null
                && mTitle != null
                && mCenterGroup != null
                && mEndGroup != null) {
            if (mIsTitleConstrainedToCenter == null || mIsTitleConstrainedToCenter != showNavEdit) {
                ConstraintSet constraintSet = new ConstraintSet();
                constraintSet.clone(mConstraintLayout);
                if (showNavEdit) {
                    constraintSet.connect(
                            R.id.pdf_title,
                            ConstraintSet.END,
                            R.id.pdf_toolbar_group_center,
                            ConstraintSet.START,
                            0);
                } else {
                    constraintSet.connect(
                            R.id.pdf_title,
                            ConstraintSet.END,
                            R.id.pdf_toolbar_group_end,
                            ConstraintSet.START,
                            0);
                }
                constraintSet.applyTo(mConstraintLayout);
                mIsTitleConstrainedToCenter = showNavEdit;
            }
        }
    }

    public boolean isDownloadButtonVisible() {
        return mDownloadButton != null && mDownloadButton.getVisibility() == View.VISIBLE;
    }

    public boolean isRotateButtonVisible() {
        return mRotateButton != null && mRotateButton.getVisibility() == View.VISIBLE;
    }

    public boolean isFitToPageButtonVisible() {
        return mFitToPageButton != null && mFitToPageButton.getVisibility() == View.VISIBLE;
    }

    private void setViewVisibility(@Nullable View view, boolean visible) {
        if (view == null) return;
        int targetVisibility = visible ? View.VISIBLE : View.GONE;
        if (view.getVisibility() != targetVisibility) {
            view.setVisibility(targetVisibility);
        }
    }

    private void setViewsVisibility(@Nullable List<View> views, boolean visible) {
        if (views == null) return;
        for (View view : views) {
            setViewVisibility(view, visible);
        }
    }

    @Override
    public void clearChildFocus(View child) {
        super.clearChildFocus(child);
        if (child.getId() == R.id.current_page) {
            hideKeyboard(child);
        }
    }

    private void hideKeyboard(View view) {
        InputMethodManager imm =
                (InputMethodManager)
                        view.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) {
            imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
        }
    }
}
