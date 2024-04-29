// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.app.PendingIntent;
import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId;

import java.util.List;

/**
 * Container for all relevant parameters to creating a customizable button list in Google Bottom
 * Bar.
 */
class BottomBarConfig {

    private final @Nullable @ButtonId Integer mSpotlightId;
    private final List<ButtonConfig> mButtonList;

    BottomBarConfig(@Nullable @ButtonId Integer spotlightId, List<ButtonConfig> buttonList) {
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
     * @return list of {@link ButtonConfig} that represents the order and configuration of the
     *     buttons to be displayed in the bottom bar.
     */
    List<ButtonConfig> getButtonList() {
        return mButtonList;
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

        ButtonConfig(@ButtonId int id, Drawable icon, String description) {
            mId = id;
            mIcon = icon;
            mDescription = description;
            mPendingIntent = null;
        }

        ButtonConfig(Context context, @ButtonId int id, CustomButtonParams params) {
            mId = id;
            mIcon = params.getIcon(context);
            mDescription = params.getDescription();
            mPendingIntent = params.getPendingIntent();
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
