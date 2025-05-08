// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.listmenu.ListMenuButton;

/** The home page button. */
@NullMarked
public class HomePageButtonView extends ListMenuButton {
    public HomePageButtonView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }
}
