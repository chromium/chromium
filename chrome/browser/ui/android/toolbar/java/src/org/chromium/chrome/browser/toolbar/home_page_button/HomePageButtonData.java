// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Data needed to show a Home Page button. */
@NullMarked
public class HomePageButtonData {
    private final OnClickListener mOnClickListener;
    private final @Nullable OnLongClickListener mOnLongClickListener;

    /**
     * Creates a new instance of HomePageButtonData
     *
     * @param onClickListener Callback when button is clicked.
     * @param onLongClickListener Callback when button is long clicked.
     */
    public HomePageButtonData(
            OnClickListener onClickListener, @Nullable OnLongClickListener onLongClickListener) {
        mOnClickListener = onClickListener;
        mOnLongClickListener = onLongClickListener;
    }

    /** Returns the {@link OnClickListener} used on the button. */
    public OnClickListener getOnClickListener() {
        return mOnClickListener;
    }

    /** Returns an optional {@link OnLongClickListener} used on the button. */
    public @Nullable OnLongClickListener getOnLongClickListener() {
        return mOnLongClickListener;
    }
}
