// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;

/** View wrapper for the @memory bottom sheet. */
@NullMarked
class AtMemoryBottomSheetView {
    private final View mContentView;

    AtMemoryBottomSheetView(Context context) {
        mContentView = LayoutInflater.from(context).inflate(R.layout.at_memory_bottom_sheet, null);
    }

    View getContentView() {
        return mContentView;
    }
}
