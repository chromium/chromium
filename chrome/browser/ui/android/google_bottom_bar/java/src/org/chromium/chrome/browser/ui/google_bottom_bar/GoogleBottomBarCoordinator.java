// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Coordinator for GoogleBottomBar module. Provides the view, and initializes various components.
 */
public class GoogleBottomBarCoordinator {

    private static final String BUTTON_LIST_PARAM = "google_bottom_bar_button_list";

    /** Returns true if GoogleBottomBar is enabled in the feature flag. */
    public static boolean isFeatureEnabled() {
        return ChromeFeatureList.sCctGoogleBottomBar.isEnabled();
    }

    private final Context mContext;
    private final GoogleBottomBarViewCreator mGoogleBottomBarViewCreator;

    /**
     * Constructor.
     *
     * @param context The associated {@link Context}.
     */
    public GoogleBottomBarCoordinator(Context context) {
        mContext = context;
        mGoogleBottomBarViewCreator =
                new GoogleBottomBarViewCreator(
                        context, BottomBarConfig.fromEncodedString(getEncodedButtonConfig()));
    }

    /** Returns a view that contains the Google Bottom bar. */
    public View createGoogleBottomBarView() {
        return mGoogleBottomBarViewCreator.createGoogleBottomBarView();
    }

    private String getEncodedButtonConfig() {
        // Chrome driven experiment - button list is obtained from Finch flag param
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR, BUTTON_LIST_PARAM);

        // TODO - implement AGA driven experiment - button list provided from Intent extra
    }
}
