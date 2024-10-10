// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.view.View;

/** Wraps calls to the Android OS Edge To Edge APIs so we can easily instrument them. */
public interface EdgeToEdgeOSWrapper {
    /**
     * Wraps {@link View#setPadding(int, int, int, int)}. Sets the padding for the given View. The
     * view may add on the space required to display the scrollbars, depending on the style and
     * visibility of the scrollbars. So the values returned from getPaddingLeft(), getPaddingTop(),
     * getPaddingRight() and getPaddingBottom() may be different from the values set in this call.
     *
     * @param view The {@link View} to pad.
     * @param left int: the left padding in pixels
     * @param top int: the top padding in pixels
     * @param right int: the right padding in pixels
     * @param bottom int: the bottom padding in pixels
     */
    void setPadding(View view, int left, int top, int right, int bottom);
}
