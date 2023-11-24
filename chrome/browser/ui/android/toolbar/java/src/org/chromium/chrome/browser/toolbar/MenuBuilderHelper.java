// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View;

import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/** A helper class for sharing common code for building a list menu button. */
public class MenuBuilderHelper {
    public static RectProvider getRectProvider(View anchorView) {
        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);
        rectProvider.setIncludePadding(true);

        int toolbarHeight = anchorView.getHeight();
        int iconHeight =
                anchorView.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        int paddingBottom = (toolbarHeight - iconHeight) / 2;
        rectProvider.setInsetPx(0, 0, 0, paddingBottom);
        return rectProvider;
    }
}
