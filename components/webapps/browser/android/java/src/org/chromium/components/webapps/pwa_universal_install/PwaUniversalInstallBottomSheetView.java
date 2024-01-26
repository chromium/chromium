// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.components.webapps.R;

/** The view portion of the PWA Universal Install bottom sheet. */
public class PwaUniversalInstallBottomSheetView {
    // The current context.
    private final Context mContext;

    // The details of the bottom sheet.
    private View mContentView;

    public PwaUniversalInstallBottomSheetView(Context context) {
        mContext = context;
    }

    public void initialize() {
        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.pwa_universal_install_bottom_sheet_content,
                                /* root= */ null);
    }

    public View getContentView() {
        return mContentView;
    }

    int getPeekHeight() {
        return mContentView.getHeight();
    }
}
