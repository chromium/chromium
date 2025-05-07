// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** The container of the two home page buttons. */
@NullMarked
public class HomePageButtonsContainerView extends LinearLayout {
    public HomePageButtonsContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }
}
