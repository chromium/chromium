// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;

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
     * @param googleBottomBarIntentParams The encoded button list provided through IntentParams
     */
    public GoogleBottomBarCoordinator(
            Context context, GoogleBottomBarIntentParams googleBottomBarIntentParams) {
        mContext = context;
        mGoogleBottomBarViewCreator =
                new GoogleBottomBarViewCreator(
                        context, getButtonConfig(googleBottomBarIntentParams));
    }

    /** Returns a view that contains the Google Bottom bar. */
    public View createGoogleBottomBarView() {
        return mGoogleBottomBarViewCreator.createGoogleBottomBarView();
    }

    private BottomBarConfig getButtonConfig(GoogleBottomBarIntentParams intentParams) {
        // Encoded button list provided in intent from embedder
        if (intentParams.getEncodedButtonCount() != 0) {
            return BottomBarConfig.fromEncodedList(intentParams.getEncodedButtonList());
        }

        // Fall back on encoded string provided in Finch param
        return BottomBarConfig.fromEncodedString(
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR, BUTTON_LIST_PARAM));
    }
}
