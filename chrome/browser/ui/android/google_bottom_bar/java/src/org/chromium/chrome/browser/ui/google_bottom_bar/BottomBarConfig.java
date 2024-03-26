// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Container for all relevant parameters to creating a customizable button list in Google Bottom
 * Bar.
 *
 * <p>The class formats the param that is received either from the Finch flag or intent extra. It
 * has the following representation "5,1,2,3,4,5", where the first item represents the spotlight
 * button and the rest of the list the order of the buttons in the bottom bar.
 */
class BottomBarConfig {

    static BottomBarConfig fromEncodedString(String encodedConfig) {
        List<Integer> encodedList = getEncodedListFromString(encodedConfig);

        return fromEncodedList(encodedList);
    }

    static BottomBarConfig fromEncodedList(List<Integer> encodedList) {
        if (encodedList.isEmpty()) {
            throw new IllegalArgumentException("The list is empty or has wrong format");
        }

        if (encodedList.size() < 2) {
            throw new IllegalArgumentException("The list doesn't have enough parameters");
        }

        List<Integer> buttonList = encodedList.subList(1, encodedList.size()); // remove spotlight

        long validButtonListSize =
                buttonList.stream().filter(BottomBarConfig::isValidButtonId).count();

        if (validButtonListSize != buttonList.size()) {
            throw new IllegalArgumentException("The list has non-valid button ids");
        }

        // 0 aka no spotlight is not encoded in ButtonId so it must be checked separately
        int spotlitButton = encodedList.get(0);

        if (!isValidButtonId(spotlitButton) && spotlitButton != 0) {
            throw new IllegalArgumentException("The spotlight button id is not supported");
        }

        return new BottomBarConfig(createSpotlight(spotlitButton), buttonList);
    }

    private final @Nullable @ButtonId Integer mSpotlightId;
    private final List<Integer> mButtonList;

    private BottomBarConfig(@Nullable @ButtonId Integer spotlightId, List<Integer> buttonList) {
        mSpotlightId = spotlightId;
        mButtonList = buttonList;
    }

    /**
     * @return the id of the spotlit button in the bottom bar or null is there is none set.
     */
    @Nullable
    @ButtonId
    Integer getSpotlightId() {
        return mSpotlightId;
    }

    /**
     * @return list of {@link ButtonId} that represents the order of the buttons to be displayed in
     *     the bottom bar.
     */
    List<Integer> getButtonList() {
        return mButtonList;
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

    /**
     * Each button is encoded as:
     * 1 - Page Insights Hub with basic icon
     * 2 - Chrome Share
     * 3 - Google App Save
     * 4 - Google App Add notes
     * 5 - Chrome Refresh
     * 6 - Page Insights Hub with coloured icon
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
    @interface ButtonId {
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
