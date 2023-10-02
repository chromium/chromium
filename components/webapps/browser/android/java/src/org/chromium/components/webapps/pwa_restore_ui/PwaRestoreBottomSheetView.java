// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;

import org.chromium.components.webapps.R;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreProperties.ViewState;

/**
 * The view portion of the PWA Install bottom sheet.
 *
 * The class is suppressing lint error for 'missing performClick override' because we don't need the
 * override. We're forwarding the event along (and the implementation of the override would be a
 * no-op for us).
 */
@SuppressLint("ClickableViewAccessibility")
public class PwaRestoreBottomSheetView implements View.OnTouchListener {
    // The current context.
    private final Context mContext;

    // The peek state for the bottom sheet.
    private View mPreviewView;

    // The details of the bottom sheet.
    private View mContentView;

    // The listener to notify when the Back button is clicked.
    private OnClickListener mBackButtonListener;

    // The back button arrow in the top bar of the content view.
    private Drawable mBackArrow;

    public PwaRestoreBottomSheetView(Context context) {
        mContext = context;
    }

    public void initialize(int backArrowId) {
        mPreviewView = LayoutInflater.from(mContext).inflate(
                R.layout.pwa_restore_bottom_sheet_preview, /* root= */ null);
        mContentView = LayoutInflater.from(mContext).inflate(
                R.layout.pwa_restore_bottom_sheet_content, /* root= */ null);

        int backgroundId = R.drawable.pwa_restore_icon;
        mPreviewView.findViewById(R.id.icon).setBackgroundResource(backgroundId);
        mPreviewView.findViewById(R.id.icon).setTag(backgroundId);
        mBackArrow = backArrowId != 0 ? ResourcesCompat.getDrawable(
                             mContext.getResources(), backArrowId, mContext.getTheme())
                                      : null;
        TextView contentViewTitle = (TextView) mContentView.findViewById(R.id.title);
        contentViewTitle.setCompoundDrawablesRelativeWithIntrinsicBounds(
                mBackArrow, null, null, null);
        contentViewTitle.setOnTouchListener(this::onTouch);
    }

    public View getContentView() {
        return mContentView;
    }

    public View getPreviewView() {
        return mPreviewView;
    }

    public void setDisplayedView(@ViewState int viewState) {
        mPreviewView.setVisibility(viewState == ViewState.VIEW_PWA_LIST ? View.GONE : View.VISIBLE);
        mContentView.setVisibility(viewState == ViewState.PREVIEW ? View.GONE : View.VISIBLE);
    }

    protected void setBackButtonListener(OnClickListener listener) {
        mBackButtonListener = listener;
    }

    // Called through the {@link PwaRestoreBottomSheetViewBinder} bindings when the property model
    // updates:

    int getPeekHeight() {
        return mPreviewView.getHeight();
    }

    @Override
    public boolean onTouch(View view, MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            MarginLayoutParams lp = (MarginLayoutParams) view.getLayoutParams();
            // Allow for a bit of margin for the hit boundary.
            if ((event.getRawX() <= mBackArrow.getIntrinsicWidth() + (2 * lp.leftMargin))) {
                return true;
            }
        } else if (event.getAction() == MotionEvent.ACTION_UP) {
            mBackButtonListener.onClick(view);
            return true;
        }
        return false;
    }
}
