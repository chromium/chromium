// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;

import org.chromium.components.webapps.R;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreProperties.ViewState;
import org.chromium.ui.util.ColorUtils;

/**
 * The view portion of the PWA Install bottom sheet.
 */
public class PwaRestoreBottomSheetView {
    // The current context.
    private final Context mContext;

    // The peek state for the bottom sheet.
    private View mPreviewView;

    // The details of the bottom sheet.
    private View mContentView;

    public PwaRestoreBottomSheetView(Context context) {
        mContext = context;
    }

    public void initialize(int backArrowId) {
        mPreviewView = LayoutInflater.from(mContext).inflate(
                R.layout.pwa_restore_bottom_sheet_preview, /* root= */ null);
        mContentView = LayoutInflater.from(mContext).inflate(
                R.layout.pwa_restore_bottom_sheet_content, /* root= */ null);

        int id = ColorUtils.inNightMode(mContext) ? R.drawable.pwa_restore_icon_dark
                                                  : R.drawable.pwa_restore_icon_light;
        mPreviewView.findViewById(R.id.icon).setBackgroundResource(id);
        mPreviewView.findViewById(R.id.icon).setTag(id);
        Drawable arrow = backArrowId != 0 ? ResourcesCompat.getDrawable(
                                 mContext.getResources(), backArrowId, mContext.getTheme())
                                          : null;
        ((TextView) mContentView.findViewById(R.id.title))
                .setCompoundDrawablesRelativeWithIntrinsicBounds(arrow, null, null, null);
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

    // Called through the {@link PwaRestoreBottomSheetViewBinder} bindings when the property model
    // updates:

    int getPeekHeight() {
        return mPreviewView.getHeight();
    }
}
