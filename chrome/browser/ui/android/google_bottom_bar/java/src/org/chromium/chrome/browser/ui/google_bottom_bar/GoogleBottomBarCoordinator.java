// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;

import java.util.Arrays;
import java.util.List;

/**
 * Coordinator for GoogleBottomBar module. Provides the view, and initializes various components.
 */
public class GoogleBottomBarCoordinator {

    private static final String BUTTON_LIST_PARAM = "google_bottom_bar_button_list";

    // TODO - move the logic into a separate class that will process information received from the
    // intent extras
    private static final List<Integer> SUPPORTED_BUTTON_IDS = Arrays.asList(100, 101, 104);

    /** Returns true if GoogleBottomBar is enabled in the feature flag. */
    public static boolean isFeatureEnabled() {
        return ChromeFeatureList.sCctGoogleBottomBar.isEnabled();
    }

    // TODO - move the logic into a separate class that will process information received from the
    // intent extras
    /** Returns true if the id of the custom button param is supported. */
    public static boolean shouldUseCustomButtonParams(int customButtonParamsId) {
        return SUPPORTED_BUTTON_IDS.contains(customButtonParamsId);
    }

    private final Context mContext;
    private final GoogleBottomBarViewCreator mGoogleBottomBarViewCreator;
    private final List<CustomButtonParams> mCustomButtonsParams;

    /**
     * Constructor.
     *
     * @param context The associated {@link Context}.
     * @param googleBottomBarIntentParams The encoded button list provided through IntentParams
     * @param customButtonsOnGoogleBottomBar List of {@link CustomButtonParams} provided by the
     *     embedder to be displayed in the Bottom Bar.
     */
    public GoogleBottomBarCoordinator(
            Context context,
            GoogleBottomBarIntentParams googleBottomBarIntentParams,
            List<CustomButtonParams> customButtonsOnGoogleBottomBar) {
        mContext = context;
        mGoogleBottomBarViewCreator =
                new GoogleBottomBarViewCreator(
                        context, getButtonConfig(googleBottomBarIntentParams));
        mCustomButtonsParams = customButtonsOnGoogleBottomBar;
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
