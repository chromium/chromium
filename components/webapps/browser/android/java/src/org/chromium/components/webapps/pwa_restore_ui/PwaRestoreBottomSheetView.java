// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.components.webapps.R;

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

    public void initialize() {
        mPreviewView = LayoutInflater.from(mContext).inflate(
                R.layout.pwa_restore_bottom_sheet_preview, /* root= */ null);
        mContentView = LayoutInflater.from(mContext).inflate(
                R.layout.pwa_restore_bottom_sheet_content, /* root= */ null);
    }

    public View getContentView() {
        return mContentView;
    }

    public View getPreviewView() {
        return mPreviewView;
    }

    // Called through the {@link PwaRestoreBottomSheetViewBinder} bindings when the property model
    // updates:

    int getPeekHeight() {
        return mPreviewView.getHeight();
    }
}
