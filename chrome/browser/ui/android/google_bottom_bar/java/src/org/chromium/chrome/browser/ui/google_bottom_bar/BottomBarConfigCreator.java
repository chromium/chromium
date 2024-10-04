// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.DOUBLE_DECKER;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.NO_VARIANT;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.SINGLE_DECKER;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonConfig;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams.VariantLayoutType;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.components.cached_flags.StringCachedFieldTrialParameter;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

/** This class creates a {@link BottomBarConfig} based on provided params. */
public class BottomBarConfigCreator {
    private static final String TAG = "GoogleBottomBar";
    private static final String BUTTON_LIST_PARAM = "google_bottom_bar_button_list";
    private static final String VARIANT_LAYOUT_PARAM = "google_bottom_bar_variant_layout";
    private static final String NO_VARIANT_HEIGHT_DP_PARAM =
            "google_bottom_bar_no_variant_height_dp";
    private static final String SINGLE_DECKER_HEIGHT_DP_PARAM =
            "google_bottom_bar_single_decker_height_dp";
    private static final String IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED_PARAM =
            "google_bottom_bar_variant_is_google_default_search_engine_check_enabled";

    @VisibleForTesting static final int DEFAULT_NO_VARIANT_HEIGHT_DP = 64;
    @VisibleForTesting static final int DEFAULT_SINGLE_DECKER_HEIGHT_DP = 62;
    @VisibleForTesting static final int DOUBLE_DECKER_HEIGHT_DP = 110;
    @VisibleForTesting static final int SINGLE_DECKER_WITH_RIGHT_BUTTONS_HEIGHT_DP = 64;

    @VisibleForTesting
    static final Map<Integer, Integer> CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP =
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
                    ButtonId.CUSTOM,
                    106,
                    ButtonId.SEARCH,
                    107,
                    ButtonId.HOME);

    private static final List<Integer> DEFAULT_BUTTON_ID_LIST =
            List.of(ButtonId.SAVE, ButtonId.SHARE);
    private static final List<Integer> DEFAULT_RIGHT_BUTTON_ID_LIST = List.of(ButtonId.SHARE);

    private final Context mContext;

    /**
     * A cached parameter representing the order of Google Bottom Bar buttons based on experiment
     * configuration.
     */
    public static final StringCachedFieldTrialParameter GOOGLE_BOTTOM_BAR_PARAM_BUTTON_LIST =
            ChromeFeatureList.newStringCachedFieldTrialParameter(
                    ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR, BUTTON_LIST_PARAM, "");

    /**
     * A cached boolean parameter to decide whether to check if Google is Chrome's default search
     * engine.
     */
    public static final BooleanCachedFieldTrialParameter
            IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED =
                    ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                            ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS,
                            IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED_PARAM,
                            false);

    /**
     * A cached parameter representing the Google Bottom Bar layout variants value based on
     * experiment configuration.
     */
    public static final IntCachedFieldTrialParameter GOOGLE_BOTTOM_BAR_VARIANT_LAYOUT_VALUE =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS,
                    VARIANT_LAYOUT_PARAM,
                    DOUBLE_DECKER);

    /**
     * A cached parameter used for specifying the height of the Google Bottom Bar in DP, when its
     * variant is NO_VARIANT.
     */
    public static final IntCachedFieldTrialParameter
            GOOGLE_BOTTOM_BAR_NO_VARIANT_HEIGHT_DP_PARAM_VALUE =
                    ChromeFeatureList.newIntCachedFieldTrialParameter(
                            ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR,
                            NO_VARIANT_HEIGHT_DP_PARAM,
                            DEFAULT_NO_VARIANT_HEIGHT_DP);

    /**
     * A cached parameter used for specifying the height of the Google Bottom Bar in DP, when its
     * variant is SINGLE_DECKER.
     */
    public static final IntCachedFieldTrialParameter
            GOOGLE_BOTTOM_BAR_SINGLE_DECKER_HEIGHT_DP_PARAM_VALUE =
                    ChromeFeatureList.newIntCachedFieldTrialParameter(
                            ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS,
                            SINGLE_DECKER_HEIGHT_DP_PARAM,
                            DEFAULT_SINGLE_DECKER_HEIGHT_DP);

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
     * Caches whether the Chrome's default search engine is Google in Chrome Shared Preferences.
     *
     * <p>This method is designed to be called after profile is available. It checks if Chrome's default
     * search engine for the given profile is Google and stores this information in Chrome Shared
     * Preferences for later retrieval.
     *
     * <p>This caching mechanism is used to optimize UI decision in {@link
     * BottomBarConfig#getLayoutType).
     *
     * @param originalProfile The profile to check. This can be null, in which case the method does
     *     nothing.
     */
    static void initDefaultSearchEngine(Profile originalProfile) {
        if (isGoogleBottomBarVariantLayoutsEnabledInFinch()
                && shouldPerformIsGoogleDefaultSearchEngineCheck()
                && originalProfile != null) {
            // Must be called on UI thread
            boolean isDefaultSearchEngineGoogle =
                    TemplateUrlServiceFactory.getForProfile(originalProfile)
                            .isDefaultSearchEngineGoogle();
            boolean storedValue =
                    ChromeSharedPreferences.getInstance()
                            .readBoolean(
                                    IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE,
                                    /* defaultValue= */ false);

            if (storedValue != isDefaultSearchEngineGoogle) {
                ChromeSharedPreferences.getInstance()
                        .writeBoolean(
                                IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE,
                                isDefaultSearchEngineGoogle);
            }
        }
    }

    /**
     * Determines which buttons to display in the Google Bottom Bar based on
     * GoogleBottomBarIntentParams.
     *
     * @param intentParams that optionally contains:
     *     <p>Integer list with the following representation [5,1,2,3,4,5], where the first item
     *     represents the spotlight button and the rest of the list the order of the buttons in the
     *     bottom bar.
     *     <p>Variant layout type that specifies variation of the layout that should be used
     * @return A set of integers representing the customButtonParamIds of the buttons that should be
     *     displayed in the Google Bottom Bar.
     */
    static Set<Integer> getSupportedCustomButtonParamIds(GoogleBottomBarIntentParams intentParams) {
        if (isGoogleBottomBarVariantLayoutsEnabled(intentParams)) {
            @GoogleBottomBarVariantLayoutType int layoutType = getLayoutType(intentParams);
            if (layoutType == SINGLE_DECKER) {
                return Set.of();
            } else if (layoutType == SINGLE_DECKER_WITH_RIGHT_BUTTONS) {
                // For Single decker layout we should return only items that are both supported and
                // in encoded button list. So that rest can be added to the toolbar.
                // Example:
                // SupportedCustomButtonParamIdList = { 100 - SAVE, 101 - SHARE, 103 - PIH_BASIC,
                // 104 - ADD_NOTES, 105 - CUSTOM, 106 - SEARCH}
                // EncodedButtonIdList = {0, SHARE, CUSTOM}
                // SupportedCustomButtonParamIds = {101 - SHARE, 105 - CUSTOM}
                // As a result SAVE button will be added to the toolbar.
                List<Integer> supportedCustomButtonParamIdList =
                        new ArrayList<>(CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet());
                List<Integer> encodedButtonIdList = getEncodedLayoutList(intentParams);
                return supportedCustomButtonParamIdList.stream()
                        .filter(
                                customButtonParamId ->
                                        encodedButtonIdList.contains(
                                                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.get(
                                                        customButtonParamId)))
                        .collect(Collectors.toSet());
            }
        }
        return CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet();
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

        int layoutType = getLayoutType(intentParams);
        List<Integer> encodedLayoutList = getEncodedLayoutList(intentParams);

        if (layoutType != NO_VARIANT && !isGoogleBottomBarVariantLayoutsSupported()) {
            if (layoutType == SINGLE_DECKER || layoutType == SINGLE_DECKER_WITH_RIGHT_BUTTONS) {
                if (encodedLayoutList.size() < 3) {

                    Log.v(
                            TAG,
                            "Can't proceed with configured variant layout: %s as Google is not"
                                    + " default search engine. Fallback to default version of"
                                    + " GoogleBottomBar with default button order.",
                            layoutType);
                    return createDefaultConfig(intentParams, customButtonParamsList, NO_VARIANT);
                }
            }
            Log.v(
                    TAG,
                    "Can't proceed with configured variant layout: %s as Google is not default"
                            + " search engine. Fallback to default version of GoogleBottomBar while"
                            + " respecting custom button order.",
                    layoutType);
            return create(intentParams, encodedLayoutList, customButtonParamsList, NO_VARIANT);
        }
        return create(intentParams, encodedLayoutList, customButtonParamsList, layoutType);
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

    /**
     * Create default {@link ButtonConfig} for the given ID. Used for buttons that have
     * implementation in Chrome.
     */
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
                // disabled save button instead
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
            case ButtonId.SEARCH:
                return new ButtonConfig(
                        id,
                        UiUtils.getTintedDrawable(
                                mContext,
                                R.drawable.ic_search,
                                R.color.default_icon_color_baseline),
                        mContext.getString(R.string.google_bottom_bar_search_button_description),
                        /* pendingIntent= */ null);
            case ButtonId.HOME:
                return new ButtonConfig(
                        id,
                        UiUtils.getTintedDrawable(
                                mContext,
                                R.drawable.bottom_bar_home_icon,
                                R.color.default_icon_color_baseline),
                        mContext.getString(R.string.google_bottom_bar_home_button_description),
                        /* pendingIntent= */ null);
            default:
                {
                    Log.e(TAG, "The ID is not supported");
                    return null;
                }
        }
    }

    private BottomBarConfig createDefaultConfig(
            GoogleBottomBarIntentParams intentParams,
            List<CustomButtonParams> customButtonParams,
            @GoogleBottomBarVariantLayoutType int variantLayoutType) {
        List<Integer> defaultButtonIdList =
                switch (variantLayoutType) {
                    case SINGLE_DECKER -> List.of();
                    case SINGLE_DECKER_WITH_RIGHT_BUTTONS -> DEFAULT_RIGHT_BUTTON_ID_LIST;
                    case DOUBLE_DECKER, NO_VARIANT -> DEFAULT_BUTTON_ID_LIST;
                    default -> {
                        Log.e(TAG, "Not valid variantLayoutType: %s", variantLayoutType);
                        yield DEFAULT_BUTTON_ID_LIST;
                    }
                };
        return new BottomBarConfig(
                /* spotlightId= */ null,
                createButtonConfigList(defaultButtonIdList, customButtonParams),
                variantLayoutType,
                getHeightDp(intentParams, variantLayoutType));
    }

    private static int getHeightDp(
            GoogleBottomBarIntentParams intentParams,
            @GoogleBottomBarVariantLayoutType int variantLayoutType) {
        return switch (variantLayoutType) {
            case DOUBLE_DECKER -> DOUBLE_DECKER_HEIGHT_DP;
            case SINGLE_DECKER -> intentParams.getSingleDeckerHeightDp() > 0
                    ? intentParams.getSingleDeckerHeightDp()
                    : GOOGLE_BOTTOM_BAR_SINGLE_DECKER_HEIGHT_DP_PARAM_VALUE.getValue();
            case SINGLE_DECKER_WITH_RIGHT_BUTTONS -> SINGLE_DECKER_WITH_RIGHT_BUTTONS_HEIGHT_DP;
            default -> intentParams.getNoVariantHeightDp() > 0
                    ? intentParams.getNoVariantHeightDp()
                    : GOOGLE_BOTTOM_BAR_NO_VARIANT_HEIGHT_DP_PARAM_VALUE.getValue();
        };
    }

    private static @GoogleBottomBarVariantLayoutType int getLayoutType(
            GoogleBottomBarIntentParams intentParams) {
        if (isGoogleBottomBarVariantLayoutsEnabledInFinch()
                && intentParams.hasVariantLayoutType()) {
            VariantLayoutType variantLayoutType = intentParams.getVariantLayoutType();
            return switch (variantLayoutType) {
                case NO_VARIANT -> NO_VARIANT;
                case CHROME_CONTROLLED -> convertFinchParamToGoogleBottomBarVariantLayoutType();
                case DOUBLE_DECKER -> DOUBLE_DECKER;
                case SINGLE_DECKER -> SINGLE_DECKER;
                case SINGLE_DECKER_WITH_RIGHT_BUTTONS -> SINGLE_DECKER_WITH_RIGHT_BUTTONS;
            };
        }
        return NO_VARIANT;
    }

    private static @GoogleBottomBarVariantLayoutType int
            convertFinchParamToGoogleBottomBarVariantLayoutType() {
        int finchParam = GOOGLE_BOTTOM_BAR_VARIANT_LAYOUT_VALUE.getValue();
        return switch (finchParam) {
            case SINGLE_DECKER -> SINGLE_DECKER;
            case SINGLE_DECKER_WITH_RIGHT_BUTTONS -> SINGLE_DECKER_WITH_RIGHT_BUTTONS;
            case DOUBLE_DECKER -> DOUBLE_DECKER;
            default -> {
                Log.e(
                        TAG,
                        "Unexpected Finch param value GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS_VALUE = %s",
                        finchParam);
                yield DOUBLE_DECKER;
            }
        };
    }

    private static List<Integer> getEncodedLayoutList(GoogleBottomBarIntentParams intentParams) {
        // If not empty, use encoded button list provided in intent from embedder,
        // otherwise fall back on encoded string provided in Finch param
        return intentParams.getEncodedButtonCount() != 0
                ? intentParams.getEncodedButtonList()
                : getEncodedListFromString(GOOGLE_BOTTOM_BAR_PARAM_BUTTON_LIST.getValue());
    }

    /**
     * Creates a {@link BottomBarConfig} object that defines the configuration of a Google Bottom
     * Bar.
     *
     * <p>This method interprets the encoded layout information, custom button parameters, and
     * variant layout type to generate a configuration object that specifies the order of buttons in
     * the Bottom Bar, including the optional "spotlight" button and layout which should be used to
     * display the Bottom Bar.
     *
     * @param intentParams that optionally contains:
     *     <p>Integer list with the following representation [5,1,2,3,4,5], where the first item
     *     represents the spotlight button and the rest of the list the order of the buttons in the
     *     bottom bar.
     *     <p>Variant layout type that specifies variation of the layout that should be used
     * @param encodedLayoutList An integer list encoding the layout of the Bottom Bar buttons. The
     *     first item is the ID of the "spotlight" button (0 - no spotlight button), followed by the
     *     IDs of the remaining buttons in their desired order. For example, `[4, 1, 2, 3, 4]`
     *     indicates button ID 4 as the spotlight while the rest of the list specifies the order in
     *     which buttons should appear in the Bottom Bar.
     * @param customButtonParams A list of {@link CustomButtonParams} objects providing additional
     *     configuration details for custom buttons.
     * @param variantLayoutType An integer constant representing the chosen variant layout type for
     *     the Google Bottom Bar (e.g., standard, compact). Refer to {@link
     *     GoogleBottomBarVariantLayoutType} for valid values.
     * @return A {@link BottomBarConfig} object that encapsulates the button order, including any
     *     spotlight button, and custom button configurations based on the provided parameters.
     *     Fallbacks to default configuration if provided parameters are not valid.
     */
    private BottomBarConfig create(
            GoogleBottomBarIntentParams intentParams,
            List<Integer> encodedLayoutList,
            List<CustomButtonParams> customButtonParams,
            @GoogleBottomBarVariantLayoutType int variantLayoutType) {

        List<Integer> buttonIdList = new ArrayList<>();
        boolean success =
                buildButtonIdListFromParams(encodedLayoutList, variantLayoutType, buttonIdList);
        if (!success) {
            Log.e(TAG, "Fallback to default bottom bar configuration.");
            return createDefaultConfig(intentParams, customButtonParams, variantLayoutType);
        }

        Integer spotlightButton =
                getSpotlightButtonFromParams(encodedLayoutList, variantLayoutType);
        if (spotlightButton == null) {
            Log.e(TAG, "Fallback to default bottom bar configuration.");
            return createDefaultConfig(intentParams, customButtonParams, variantLayoutType);
        }

        return new BottomBarConfig(
                createSpotlight(spotlightButton),
                createButtonConfigList(buttonIdList, customButtonParams),
                variantLayoutType,
                getHeightDp(intentParams, variantLayoutType));
    }

    /**
     * Builds a list of button IDs (`buttonIdList`) based on the encoded layout
     * (`encodedLayoutList`) and the Google Bottom Bar variant layout type (`variantLayoutType`).
     *
     * @param encodedLayoutList A list of integers representing the encoded layout of the buttons.
     * @param variantLayoutType An integer representing the Google Bottom Bar variant layout type.
     *     See {@link GoogleBottomBarVariantLayoutType} for possible values.
     * @param buttonIdList An empty list that will be filled with valid {@link ButtonId} elements.
     * @return true if the method successfully built a valid `buttonIdList`, false otherwise.
     */
    private boolean buildButtonIdListFromParams(
            List<Integer> encodedLayoutList,
            @GoogleBottomBarVariantLayoutType int variantLayoutType,
            List<Integer> buttonIdList) {
        if (variantLayoutType == SINGLE_DECKER) {
            if (!encodedLayoutList.isEmpty()) {
                Log.e(TAG, "Single decker doesn't support additional buttons.");
                return false;
            }
            return true;
        }

        if (encodedLayoutList.isEmpty()) {
            Log.e(TAG, "The list is empty or has wrong format");
            return false;
        }

        if (encodedLayoutList.size() < 2) {
            Log.e(TAG, "The list doesn't have enough parameters");
            return false;
        }

        buttonIdList.addAll(
                encodedLayoutList.subList(1, encodedLayoutList.size())); // remove spotlight

        if (variantLayoutType == SINGLE_DECKER_WITH_RIGHT_BUTTONS && buttonIdList.size() > 2) {
            Log.e(
                    TAG,
                    "The single decker with right buttons layout doesn't support more than 2"
                            + " elements.");
            return false;
        }

        long validButtonListSize =
                buttonIdList.stream().filter(BottomBarConfigCreator::isValidButtonId).count();

        if (validButtonListSize != buttonIdList.size()) {
            Log.e(TAG, "The list has non-valid button ids");
            return false;
        }
        return true;
    }

    /**
     * Determines the ID of the "spotlight" button within a Google Bottom Bar configuration.
     *
     * <p>The spotlight button is a visually prominent button in the Google Bottom Bar. Its presence
     * and ID are determined by the encoded layout (`encodedLayoutList`) and the Google Bottom Bar
     * variant layout type (`variantLayoutType`).
     *
     * @param encodedLayoutList A list of integers representing the encoded layout configuration for
     *     the Google Bottom Bar buttons.
     * @param variantLayoutType An integer representing the Google Bottom Bar variant layout type.
     *     See {@link GoogleBottomBarVariantLayoutType} for possible values.
     * @return
     *     <ul>
     *       <li>If a valid "spotlight" button is found in the given configuration: Returns its ID,
     *           which is an integer value from the {@link ButtonId} enum.
     *       <li>If no "spotlight" button exists in the given configuration: Returns 0.
     *       <li>If an error occurs during processing: Returns null.
     *     </ul>
     */
    private Integer getSpotlightButtonFromParams(
            List<Integer> encodedLayoutList,
            @GoogleBottomBarVariantLayoutType int variantLayoutType) {
        if (variantLayoutType == SINGLE_DECKER) {
            if (!encodedLayoutList.isEmpty()) {
                Log.e(TAG, "Single decker doesn't support spotlight button.");
                return null;
            }
            return 0;
        } else if (encodedLayoutList.isEmpty() || encodedLayoutList.size() < 2) {
            Log.e(TAG, "Spotlight button is not specified in encoded layout list.");
            return null;
        } else {
            int spotlightButton = encodedLayoutList.get(0);

            if (spotlightButton != 0) {
                if (variantLayoutType == SINGLE_DECKER_WITH_RIGHT_BUTTONS) {
                    Log.e(
                            TAG,
                            "Single decker with right buttons layout doesn't support spotlight"
                                    + " button.");
                    return null;
                }
                if (!isValidButtonId(spotlightButton)) {
                    Log.e(TAG, "The spotlight button id is not valid %s.", spotlightButton);
                    return null;
                }
            }
            return spotlightButton;
        }
    }

    @Nullable
    private static @ButtonId Integer createSpotlight(int code) {
        return code != 0 ? code : null;
    }

    private static boolean isGoogleBottomBarVariantLayoutsEnabledInFinch() {
        return ChromeFeatureList.sCctGoogleBottomBarVariantLayouts.isEnabled();
    }

    private static boolean isGoogleBottomBarVariantLayoutsEnabled(
            GoogleBottomBarIntentParams intentParams) {
        return isGoogleBottomBarVariantLayoutsEnabledInFinch()
                && isGoogleBottomBarVariantLayoutsSupported()
                && intentParams.hasVariantLayoutType()
                && !intentParams.getVariantLayoutType().equals(VariantLayoutType.NO_VARIANT);
    }

    private static boolean shouldPerformIsGoogleDefaultSearchEngineCheck() {
        return IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.getValue();
    }

    private static boolean isGoogleBottomBarVariantLayoutsSupported() {
        if (shouldPerformIsGoogleDefaultSearchEngineCheck()) {
            boolean isSupported =
                    isGoogleBottomBarVariantLayoutsEnabledInFinch()
                            && ChromeSharedPreferences.getInstance()
                                    .readBoolean(
                                            IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE,
                                            /* defaultValue= */ false);
            Log.v(TAG, "isGoogleBottomBarVariantLayoutsSupported: %s", isSupported);
            return isSupported;
        }
        return isGoogleBottomBarVariantLayoutsEnabledInFinch();
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
        return switch (buttonId) {
            case ButtonId.PIH_BASIC, ButtonId.PIH_COLORED, ButtonId.PIH_EXPANDED ->
            // Always use pageInsights icon provided by Chrome
            UiUtils.getTintedDrawable(
                    context,
                    R.drawable.bottom_bar_page_insights_icon,
                    R.color.default_icon_color_baseline);
            case ButtonId.SEARCH ->
            // Always use search icon provided by Chrome
            UiUtils.getTintedDrawable(
                    context, R.drawable.ic_search, R.color.default_icon_color_baseline);
            case ButtonId.HOME -> UiUtils.getTintedDrawable(
                    context, R.drawable.bottom_bar_home_icon, R.color.default_icon_color_baseline);
            default -> getTintedIcon(
                    context, params.getIcon(context), R.color.default_icon_color_baseline);
        };
    }
}
