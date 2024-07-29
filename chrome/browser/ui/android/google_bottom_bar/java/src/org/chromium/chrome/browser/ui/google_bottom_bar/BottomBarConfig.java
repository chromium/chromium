// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.app.PendingIntent;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Container for all relevant parameters to creating a customizable button list in Google Bottom
 * Bar.
 */
class BottomBarConfig {

    // LINT.IfChange
    /** Defines the possible variant layout types for the Google Bottom Bar. */
    @IntDef({
        GoogleBottomBarVariantLayoutType.NO_VARIANT,
        GoogleBottomBarVariantLayoutType.DOUBLE_DECKER,
        GoogleBottomBarVariantLayoutType.SINGLE_DECKER,
        GoogleBottomBarVariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface GoogleBottomBarVariantLayoutType {
        int NO_VARIANT = 0;
        int DOUBLE_DECKER = 1;
        int SINGLE_DECKER = 2;
        int SINGLE_DECKER_WITH_RIGHT_BUTTONS = 3;
    }

    // LINT.ThenChange(//chrome/browser/about_flags.cc)

    /**
     * Each button is encoded as: 1 - Page Insights Hub with basic icon 2 - Chrome Share 3 - Save 4
     * - Add notes 5 - Chrome Refresh 6 - Page Insights Hub with coloured icon 7 - Page Insights Hub
     * with expanded icon 8 - Custom button 9 - Search button
     */
    @IntDef({
        ButtonId.PIH_BASIC,
        ButtonId.SHARE,
        ButtonId.SAVE,
        ButtonId.ADD_NOTES,
        ButtonId.REFRESH,
        ButtonId.PIH_COLORED,
        ButtonId.PIH_EXPANDED,
        ButtonId.CUSTOM,
        ButtonId.SEARCH,
        ButtonId.HOME,
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
        int CUSTOM = 8;
        int SEARCH = 9;
        int HOME = 10;
        int MAX_BUTTON_ID = HOME;
    }

    private final @GoogleBottomBarVariantLayoutType int mVariantLayoutType;
    private final @Nullable @ButtonId Integer mSpotlightId;
    private final List<ButtonConfig> mButtonList;
    private final int mHeightDp;

    BottomBarConfig(
            @Nullable @ButtonId Integer spotlightId,
            List<ButtonConfig> buttonList,
            @GoogleBottomBarVariantLayoutType int variantLayoutType,
            int heightDp) {
        mSpotlightId = spotlightId;
        mButtonList = buttonList;
        mVariantLayoutType = variantLayoutType;
        mHeightDp = heightDp;
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
     * @return list of {@link ButtonConfig} that represents the order and configuration of the
     *     buttons to be displayed in the bottom bar.
     */
    List<ButtonConfig> getButtonList() {
        return mButtonList;
    }

    /**
     * @return The variant layout type that should be used to display Google bottom bar.
     */
    @GoogleBottomBarVariantLayoutType
    int getVariantLayoutType() {
        return mVariantLayoutType;
    }

    /**
     * @return the height of the bottom bar in DP.
     */
    int getHeightDp() {
        return mHeightDp;
    }

    /**
     * Class that represents the configuration of a Button. It contains a {@link ButtonId} and if it
     * exists, {@link CustomButtonParams} provided through intent extra by the client.
     */
    static class ButtonConfig {

        private final @ButtonId int mId;

        private final Drawable mIcon;

        private final String mDescription;

        private final @Nullable PendingIntent mPendingIntent;

        ButtonConfig(
                @ButtonId int id,
                Drawable icon,
                String description,
                @Nullable PendingIntent pendingIntent) {
            mId = id;
            mIcon = icon;
            mDescription = description;
            mPendingIntent = pendingIntent;
        }

        public @ButtonId int getId() {
            return mId;
        }

        public Drawable getIcon() {
            return mIcon;
        }

        public String getDescription() {
            return mDescription;
        }

        @Nullable
        public PendingIntent getPendingIntent() {
            return mPendingIntent;
        }
    }
}
