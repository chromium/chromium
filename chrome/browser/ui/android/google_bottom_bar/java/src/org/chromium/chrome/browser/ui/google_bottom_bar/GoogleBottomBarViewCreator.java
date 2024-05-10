// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonConfig;

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
     * @return empty view. TODO: build view dynamically based on config
     */
    public View createGoogleBottomBarView() {
        if (mConfig.getSpotlightId() != null) {
            return createGoogleBottomBarSpotlightLayoutView();
        } else {
            return createGoogleBottomBarEvenLayoutView();
        }
    }

    private View createGoogleBottomBarEvenLayoutView() {
        return
            LayoutInflater.from(mContext)
                .inflate(R.layout.google_bottom_bar_even, null);
    }

    private View createGoogleBottomBarSpotlightLayoutView() {
        return LayoutInflater.from(mContext).inflate(R.layout.google_bottom_bar_spotlight, null);
    }

    public boolean updateBottomBarButton(ButtonConfig buttonConfig) {
        // TODO Add logic to update button
        return buttonConfig != null;
    }
}
