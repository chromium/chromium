// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

/** Builds the GoogleBottomBar view. */
public class GoogleBottomBarViewCreator {
    private final Context mContext;
    private final BottomBarConfig mConfig;

    /**
     * Constructor.
     *
     * @param context An Android context.
     * @param config Bottom bar configuration for the buttons that will be displayed.
     */
    public GoogleBottomBarViewCreator(Context context, BottomBarConfig config) {
        mContext = context;
        mConfig = config;
    }

    /**
     * @return empty view. TODO - replace with actual implementation
     */
    public View createGoogleBottomBarView() {
        LinearLayout parent = new LinearLayout(mContext);

        parent.setLayoutParams(
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        parent.setOrientation(LinearLayout.HORIZONTAL);
        return parent;
    }
}
