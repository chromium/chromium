// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.content.Context;
import android.view.View;

import org.chromium.base.cached_flags.StringCachedFieldTrialParameter;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;

import java.util.List;

/**
 * Coordinator for GoogleBottomBar module. Provides the view, and initializes various components.
 */
public class GoogleBottomBarCoordinator {

    private static final String BUTTON_LIST_PARAM = "google_bottom_bar_button_list";

    public static final StringCachedFieldTrialParameter GOOGLE_BOTTOM_BAR_PARAM_BUTTON_LIST =
            ChromeFeatureList.newStringCachedFieldTrialParameter(
                    ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR, BUTTON_LIST_PARAM, "");

    /** Returns true if GoogleBottomBar is enabled in the feature flag. */
    public static boolean isFeatureEnabled() {
        return ChromeFeatureList.sCctGoogleBottomBar.isEnabled();
    }

    /** Returns true if the id of the custom button param is supported. */
    public static boolean isSupported(int customButtonParamsId) {
        return BottomBarConfigCreator.shouldAddToGoogleBottomBar(customButtonParamsId);
    }

    private final Context mContext;
    private final GoogleBottomBarViewCreator mGoogleBottomBarViewCreator;

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
                        context,
                        getButtonConfig(
                                googleBottomBarIntentParams, customButtonsOnGoogleBottomBar));
    }

    /**
     * Updates the state of a bottom bar button based on the provided parameters.
     *
     * @param params The parameters containing information relevant to the button update.
     * @return {@code true} if the button was successfully updated, {@code false} otherwise.
     */
    public boolean updateBottomBarButton(CustomButtonParams params) {
        return mGoogleBottomBarViewCreator.updateBottomBarButton(
                BottomBarConfigCreator.createButtonConfigFromCustomParams(mContext, params));
    }

    /** Returns a view that contains the Google Bottom bar. */
    public View createGoogleBottomBarView() {
        return mGoogleBottomBarViewCreator.createGoogleBottomBarView();
    }

    /** Returns the height of the Google Bottom bar in pixels. */
    public static int getBottomBarHeightInPx(Context context) {
        return context.getResources().getDimensionPixelSize(R.dimen.google_bottom_bar_height);
    }

    private BottomBarConfig getButtonConfig(
            GoogleBottomBarIntentParams intentParams,
            List<CustomButtonParams> customButtonsOnGoogleBottomBar) {
        BottomBarConfigCreator configCreator = new BottomBarConfigCreator(mContext);

        // Encoded button list provided in intent from embedder
        if (intentParams.getEncodedButtonCount() != 0) {
            return configCreator.create(
                    intentParams.getEncodedButtonList(), customButtonsOnGoogleBottomBar);
        }

        // Fall back on encoded string provided in Finch param
        return configCreator.create(
                GOOGLE_BOTTOM_BAR_PARAM_BUTTON_LIST.getValue(), customButtonsOnGoogleBottomBar);
    }
}
