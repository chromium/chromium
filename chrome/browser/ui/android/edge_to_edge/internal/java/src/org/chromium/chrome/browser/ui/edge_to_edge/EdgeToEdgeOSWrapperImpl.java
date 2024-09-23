// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.view.View;
import android.view.Window;

import androidx.annotation.NonNull;
import androidx.core.view.WindowCompat;

/** Wraps calls to the Android OS Edge To Edge APIs so we can easily instrument them. */
public class EdgeToEdgeOSWrapperImpl implements EdgeToEdgeOSWrapper {
    @Override
    public void setDecorFitsSystemWindows(@NonNull Window window, boolean decorFitsSystemWindows) {
        WindowCompat.setDecorFitsSystemWindows(window, decorFitsSystemWindows);
    }

    @Override
    public void setPadding(View view, int left, int top, int right, int bottom) {
        view.setPadding(left, top, right, bottom);
    }
}
