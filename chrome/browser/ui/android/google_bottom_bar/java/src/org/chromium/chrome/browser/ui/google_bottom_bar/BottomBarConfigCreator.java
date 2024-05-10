// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonConfig;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

/** This class creates a {@link BottomBarConfig} based on provided params. */
public class BottomBarConfigCreator {
    private static final String TAG = "GoogleBottomBar";
    private static final Map<Integer, Integer> CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP =
            Map.of(
                    100,
                    ButtonId.SAVE,
                    101,
                    ButtonId.SHARE,
                    103,
                    ButtonId.PIH_BASIC,
                    104,
                    ButtonId.ADD_NOTES);

    private final Context mContext;

    /** Returns true if the id of the custom button param is supported. */
    static boolean shouldAddToGoogleBottomBar(int customButtonParamsId) {
        return CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.containsKey(customButtonParamsId);
    }

    /**
     * Creates a ButtonConfig object based on the provided {@link CustomButtonParams}.
     *
     * @param context The Android Context.
     * @param params The custom parameters for the button configuration.
     * @return {@link BottomBarConfig}, or null if creation is not possible.
     */
    static @Nullable ButtonConfig createButtonConfigFromCustomParams(
            Context context, CustomButtonParams params) {
        Integer id = getButtonId(params.getId());
        if (id != null) {
            return new ButtonConfig(context, id, params);
        }
        return null;
    }

    /**
     * @param encodedLayout String with the following representation: "5,1,2,3,4,5", where the first
     *     item represents the spotlight button and the rest of the list the order of the buttons in
     *     the bottom bar.
     * @param customButtonParams Parameters for custom buttons provided by the client
     * @return {@link BottomBarConfig} that contains an ordered list of the buttons and the
     *     spotlight if available
     */
    BottomBarConfig create(String encodedLayout, List<CustomButtonParams> customButtonParams) {
        List<Integer> encodedList = getEncodedListFromString(encodedLayout);

        return create(encodedList, customButtonParams);
    }

    /**
     * @param encodedLayoutList Integer list with the following representation [5,1,2,3,4,5], where
     *     the first item represents the spotlight button and the rest of the list the order of the
     *     buttons in the bottom bar.
     * @param customButtonParams Parameters for custom buttons provided by the client
     * @return {@link BottomBarConfig} that contains an ordered list of the buttons and the
     *     spotlight if available
     */
    BottomBarConfig create(
            List<Integer> encodedLayoutList, List<CustomButtonParams> customButtonParams) {
        if (encodedLayoutList.isEmpty()) {
            throw new IllegalArgumentException("The list is empty or has wrong format");
        }

        if (encodedLayoutList.size() < 2) {
            throw new IllegalArgumentException("The list doesn't have enough parameters");
        }

        List<Integer> buttonIdList =
                encodedLayoutList.subList(1, encodedLayoutList.size()); // remove spotlight

        long validButtonListSize =
                buttonIdList.stream().filter(BottomBarConfigCreator::isValidButtonId).count();

        if (validButtonListSize != buttonIdList.size()) {
            throw new IllegalArgumentException("The list has non-valid button ids");
        }

        // 0 aka no spotlight is not encoded in ButtonId so it must be checked separately
        int spotlitButton = encodedLayoutList.get(0);

        if (!isValidButtonId(spotlitButton) && spotlitButton != 0) {
            throw new IllegalArgumentException("The spotlight button id is not supported");
        }

        return new BottomBarConfig(
                createSpotlight(spotlitButton),
                createButtonConfigList(buttonIdList, customButtonParams));
    }

    BottomBarConfigCreator(Context context) {
        mContext = context;
    }

    @Nullable
    private static @ButtonId Integer createSpotlight(int code) {
        return code != 0 ? code : null;
    }

    private List<ButtonConfig> createButtonConfigList(
            List<Integer> buttonIdList, List<CustomButtonParams> customButtonParams) {
        List<ButtonConfig> buttonConfigs = new ArrayList<>();

        for (@ButtonId int id : buttonIdList) {
            ButtonConfig buttonConfig =
                    createButtonConfigFromCustomParamsList(customButtonParams, id);
            // If we don't succeed to create button from custom params, fallback to default version
            if (buttonConfig == null) {
                buttonConfig = createButtonConfigFromId(id);
            }
            if (buttonConfig != null) {
                buttonConfigs.add(buttonConfig);
            }
        }
        return buttonConfigs;
    }

    private @Nullable ButtonConfig createButtonConfigFromCustomParamsList(
            List<CustomButtonParams> customButtonParams, @ButtonId int id) {

        for (CustomButtonParams params : customButtonParams) {
            Integer buttonId = getButtonId(params.getId());
            if (buttonId == id) {
                return new ButtonConfig(mContext, buttonId, params);
            }
        }
        return null;
    }

    private static List<Integer> getEncodedListFromString(String encodedConfig) {
        List<Integer> result;

        try {
            result =
                    Arrays.stream(encodedConfig.split(","))
                            .mapToInt(Integer::parseInt)
                            .boxed()
                            .collect(Collectors.toList());
        } catch (NumberFormatException e) {
            result = Collections.emptyList();
        }

        return result;
    }

    private static @ButtonId Integer getButtonId(int customButtonParamId) {
        return CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.get(customButtonParamId);
    }

    /**
     * {@link ButtonId.SAVE} and {@link ButtonId.ADD_NOTES} already receive all button configuration
     * from the client, so they should never reach this switch statement.
     *
     * <p>Create a {@link ButtonConfig} from existing resources. TODO - when the icons and
     * descriptions are defined in the codebase, update the implementation for each individual
     * button
     */
    private static @Nullable ButtonConfig createButtonConfigFromId(@ButtonId int id) {
        switch (id) {
            case ButtonId.PIH_BASIC,
                    ButtonId.SHARE,
                    ButtonId.REFRESH,
                    ButtonId.PIH_COLORED,
                    ButtonId.PIH_EXPANDED:
                return new ButtonConfig(id, null, "");
            default:
                {
                    Log.e(TAG, "The ID is not supported");
                    return null;
                }
        }
    }

    /**
     * Each button is encoded as: 1 - Page Insights Hub with basic icon 2 - Chrome Share 3 - Google
     * App Save 4 - Google App Add notes 5 - Chrome Refresh 6 - Page Insights Hub with coloured icon
     * 7 - Page Insights Hub with expanded icon
     */
    @IntDef({
        ButtonId.PIH_BASIC,
        ButtonId.SHARE,
        ButtonId.SAVE,
        ButtonId.ADD_NOTES,
        ButtonId.REFRESH,
        ButtonId.PIH_COLORED,
        ButtonId.PIH_EXPANDED,
        ButtonId.MAX_BUTTON_ID,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonId {
        int PIH_BASIC = 1;
        int SHARE = 2;
        int SAVE = 3;
        int ADD_NOTES = 4;
        int REFRESH = 5;
        int PIH_COLORED = 6;
        int PIH_EXPANDED = 7;
        int MAX_BUTTON_ID = PIH_EXPANDED;
    }

    /**
     * @param code encoded code received as param
     * @return True if button is a valid {@link ButtonId}.
     */
    private static boolean isValidButtonId(int code) {
        return code > 0 && code <= ButtonId.MAX_BUTTON_ID;
    }
}
