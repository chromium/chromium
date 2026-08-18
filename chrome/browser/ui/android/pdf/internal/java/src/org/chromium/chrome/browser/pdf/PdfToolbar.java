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
import org.chromium.ui.util.CommonOnLayoutChangeListeners;

import java.util.Arrays;
import java.util.List;

/**
 * Toolbar for the PDF viewer. To handle clicks on the navigation icon, set a listener with {@link
 * #setNavigationOnClickListener(OnClickListener)}.
 */
@NullMarked
public class PdfToolbar extends Toolbar {
    /** Listener for width changes of the toolbar. */
    public interface OnWidthChangedListener {
        void onWidthChanged(int widthPx);
    }

    private @Nullable OnWidthChangedListener mOnWidthChangedListener;

    public void setOnWidthChangedListener(@Nullable OnWidthChangedListener listener) {
        mOnWidthChangedListener = listener;
    }
    private @Nullable View mDownloadButton;
    private @Nullable View mDoneButton;
    private @Nullable View mFitToPageButton;
    private @Nullable List<View> mZoomControls;
    private @Nullable List<View> mPageNav;
    private @Nullable View mEditButton;

    private @Nullable View mNavZoomDivider;
    private @Nullable View mZoomFitDivider;
    private @Nullable View mFitEditDivider;

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
        mDoneButton = findViewById(R.id.done_button);
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
        if (!PdfUtils.isInlinePdfV2EditEnabled()) {
            setViewVisibility(mEditButton, false);
        }

        mNavZoomDivider = findViewById(R.id.nav_zoom_divider);
        mZoomFitDivider = findViewById(R.id.zoom_fit_divider);
        mFitEditDivider = findViewById(R.id.fit_edit_divider);

        mConstraintLayout = findViewById(R.id.pdf_toolbar_layout);
        mCenterGroup = findViewById(R.id.pdf_toolbar_group_center);
        mEndGroup = findViewById(R.id.pdf_toolbar_group_end);
        mTitle = findViewById(R.id.pdf_title);

        updateDividersAndConstraints();

        addOnLayoutChangeListener(
                CommonOnLayoutChangeListeners.createWidthChangedListener(
                        (v, left, top, right, bottom) -> {
                            if (mOnWidthChangedListener != null) {
                                mOnWidthChangedListener.onWidthChanged(right - left);
                            }
                        }));
    }

    void setDownloadButtonVisible(boolean visible) {
        setViewVisibility(mDownloadButton, visible);
        updateDividersAndConstraints();
    }

    void setDoneButtonVisible(boolean visible) {
        setViewVisibility(mDoneButton, visible);
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
        setViewVisibility(mEditButton, visible && PdfUtils.isInlinePdfV2EditEnabled());
        updateDividersAndConstraints();
    }

    private void updateDividersAndConstraints() {
        boolean showPageNav =
                mPageNav != null
                        && !mPageNav.isEmpty()
                        && mPageNav.get(0).getVisibility() == View.VISIBLE;
        boolean showEdit = mEditButton != null && mEditButton.getVisibility() == View.VISIBLE;
        boolean showZoom =
                mZoomControls != null
                        && !mZoomControls.isEmpty()
                        && mZoomControls.get(0).getVisibility() == View.VISIBLE;
        boolean showFit =
                mFitToPageButton != null && mFitToPageButton.getVisibility() == View.VISIBLE;

        // Dividers
        setViewVisibility(mNavZoomDivider, showPageNav && showZoom);
        setViewVisibility(mZoomFitDivider, showZoom && showFit);
        setViewVisibility(mFitEditDivider, showFit && showEdit);

        boolean isCenterGroupVisible = showPageNav || showZoom || showFit || showEdit;
        setViewVisibility(mCenterGroup, isCenterGroupVisible);

        // Adjust title constraints
        if (mConstraintLayout != null
                && mTitle != null
                && mCenterGroup != null
                && mEndGroup != null) {
            if (mIsTitleConstrainedToCenter == null
                    || mIsTitleConstrainedToCenter != isCenterGroupVisible) {
                ConstraintSet constraintSet = new ConstraintSet();
                constraintSet.clone(mConstraintLayout);
                if (isCenterGroupVisible) {
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
                mIsTitleConstrainedToCenter = isCenterGroupVisible;
            }
        }
    }

    public boolean isDownloadButtonVisible() {
        return mDownloadButton != null && mDownloadButton.getVisibility() == View.VISIBLE;
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
