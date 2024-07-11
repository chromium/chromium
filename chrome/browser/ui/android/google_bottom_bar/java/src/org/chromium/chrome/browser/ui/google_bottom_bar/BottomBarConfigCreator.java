// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.cached_flags.StringCachedFieldTrialParameter;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonConfig;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

/** This class creates a {@link BottomBarConfig} based on provided params. */
public class BottomBarConfigCreator {
    private static final String TAG = "GoogleBottomBar";
    private static final String BUTTON_LIST_PARAM = "google_bottom_bar_button_list";
    private static final Map<Integer, Integer> CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP =
            Map.of(
                    100,
                    ButtonId.SAVE,
                    101,
                    ButtonId.SHARE,
                    103,
                    ButtonId.PIH_BASIC,
                    104,
                    ButtonId.ADD_NOTES,
                    105,
                    ButtonId.CUSTOM);
    private static final List<Integer> DEFAULT_BUTTON_ID_LIST =
            List.of(ButtonId.SAVE, ButtonId.PIH_BASIC, ButtonId.SHARE);

    private final Context mContext;

    /**
     * A cached parameter representing the order of Google Bottom Bar buttons based on experiment
     * configuration.
     */
    public static final StringCachedFieldTrialParameter GOOGLE_BOTTOM_BAR_PARAM_BUTTON_LIST =
            ChromeFeatureList.newStringCachedFieldTrialParameter(
                    ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR, BUTTON_LIST_PARAM, "");

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
        Integer buttonId = getButtonId(params.getId());
        if (buttonId != null) {
            return getButtonConfigFromCustomButtonParams(context, buttonId, params);
        }
        return null;
    }

    /**
     * @param intentParams that optionally contains:
     *     <p>Integer list with the following representation [5,1,2,3,4,5], where the first item
     *     represents the spotlight button and the rest of the list the order of the buttons in the
     *     bottom bar.
     *     <p>Variant layout type that specifies variation of the layout that should be used
     * @param customButtonParamsList Parameters for custom buttons provided by the client
     * @return {@link BottomBarConfig} that contains an ordered list of the buttons, the
     *     spotlight(if available) and variant layout type(if available)
     */
    BottomBarConfig create(
            GoogleBottomBarIntentParams intentParams,
            List<CustomButtonParams> customButtonParamsList) {

        List<Integer> encodedLayoutList =
                intentParams.getEncodedButtonCount() != 0
                        // Encoded button list provided in intent from embedder
                        ? intentParams.getEncodedButtonList()
                        :
                        // Fall back on encoded string provided in Finch param
                        getEncodedListFromString(GOOGLE_BOTTOM_BAR_PARAM_BUTTON_LIST.getValue());
        return create(encodedLayoutList, customButtonParamsList);
    }

    BottomBarConfigCreator(Context context) {
        mContext = context;
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
                return getButtonConfigFromCustomButtonParams(mContext, buttonId, params);
            }
        }
        return null;
    }

    /** Create default {@link ButtonConfig} for the given ID. */
    private @Nullable ButtonConfig createButtonConfigFromId(@ButtonId int id) {
        switch (id) {
            case ButtonId.PIH_BASIC, ButtonId.PIH_COLORED, ButtonId.PIH_EXPANDED:
                return new ButtonConfig(
                        id,
                        UiUtils.getTintedDrawable(
                                mContext,
                                R.drawable.bottom_bar_page_insights_icon,
                                R.color.default_icon_color_baseline),
                        mContext.getString(
                                R.string.google_bottom_bar_page_insights_button_description),
                        /* pendingIntent= */ null);
            case ButtonId.SAVE:
                // If save button is not created from embedder-provided CustomButtonParams, provide
                // disabled save
                // button instead
                return new ButtonConfig(
                        id,
                        UiUtils.getTintedDrawable(
                                mContext, R.drawable.bookmark, R.color.default_icon_color_disabled),
                        mContext.getString(
                                R.string.google_bottom_bar_save_disabled_button_description),
                        /* pendingIntent= */ null);
            case ButtonId.SHARE:
                return new ButtonConfig(
                        id,
                        UiUtils.getTintedDrawable(
                                mContext,
                                R.drawable.ic_share_white_24dp,
                                R.color.default_icon_color_baseline),
                        mContext.getString(R.string.google_bottom_bar_share_button_description),
                        /* pendingIntent= */ null);
            default:
                {
                    Log.e(TAG, "The ID is not supported");
                    return null;
                }
        }
    }

    private BottomBarConfig createDefaultConfig(List<CustomButtonParams> customButtonParams) {
        return new BottomBarConfig(
                /* spotlightId= */ null,
                createButtonConfigList(DEFAULT_BUTTON_ID_LIST, customButtonParams));
    }

    /**
     * @param encodedLayoutList Integer list with the following representation [5,1,2,3,4,5], where
     *     the first item represents the spotlight button and the rest of the list the order of the
     *     buttons in the bottom bar.
     * @param customButtonParams Parameters for custom buttons provided by the client
     * @return {@link BottomBarConfig} that contains an ordered list of the buttons and the
     *     spotlight if available
     */
    private BottomBarConfig create(
            List<Integer> encodedLayoutList, List<CustomButtonParams> customButtonParams) {
        if (encodedLayoutList.isEmpty()) {
            Log.e(TAG, "The list is empty or has wrong format");
            return createDefaultConfig(customButtonParams);
        }

        if (encodedLayoutList.size() < 2) {
            Log.e(TAG, "The list doesn't have enough parameters");
            return createDefaultConfig(customButtonParams);
        }

        List<Integer> buttonIdList =
                encodedLayoutList.subList(1, encodedLayoutList.size()); // remove spotlight

        long validButtonListSize =
                buttonIdList.stream().filter(BottomBarConfigCreator::isValidButtonId).count();

        if (validButtonListSize != buttonIdList.size()) {
            Log.e(TAG, "The list has non-valid button ids");
            return createDefaultConfig(customButtonParams);
        }

        // 0 aka no spotlight is not encoded in ButtonId so it must be checked separately
        int spotlightButton = encodedLayoutList.get(0);

        if (!isValidButtonId(spotlightButton) && spotlightButton != 0) {
            Log.e(TAG, "The spotlight button id is not supported");
            return createDefaultConfig(customButtonParams);
        }

        return new BottomBarConfig(
                createSpotlight(spotlightButton),
                createButtonConfigList(buttonIdList, customButtonParams));
    }

    @Nullable
    private static @ButtonId Integer createSpotlight(int code) {
        return code != 0 ? code : null;
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
     * @param code encoded code received as param
     * @return True if button is a valid {@link ButtonId}.
     */
    private static boolean isValidButtonId(int code) {
        return code > 0 && code <= ButtonId.MAX_BUTTON_ID;
    }

    private static Drawable getTintedIcon(Context context, Drawable drawable, int tintColorId) {
        drawable.setTint(context.getColor(tintColorId));
        return drawable;
    }

    private static ButtonConfig getButtonConfigFromCustomButtonParams(
            Context context, int buttonId, CustomButtonParams params) {
        return new ButtonConfig(
                buttonId,
                getIconDrawable(context, buttonId, params),
                params.getDescription(),
                params.getPendingIntent());
    }

    private static Drawable getIconDrawable(
            Context context, @ButtonId int buttonId, CustomButtonParams params) {
        // Always use pageInsights icon provided by Chrome
        return isPageInsightsButton(buttonId)
                ? UiUtils.getTintedDrawable(
                        context,
                        R.drawable.bottom_bar_page_insights_icon,
                        R.color.default_icon_color_baseline)
                : getTintedIcon(
                        context, params.getIcon(context), R.color.default_icon_color_baseline);
    }

    private static boolean isPageInsightsButton(@ButtonId int buttonId) {
        return buttonId == ButtonId.PIH_BASIC
                || buttonId == ButtonId.PIH_COLORED
                || buttonId == ButtonId.PIH_EXPANDED;
    }
}
