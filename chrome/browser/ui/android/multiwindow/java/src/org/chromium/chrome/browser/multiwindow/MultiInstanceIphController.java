// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Controller to manage when and how we show multi-instance in-product help messages to users. */
public class MultiInstanceIphController {
    /**
     * Attempts to show an IPH text bubble about the instance swicher in app menu.
     *
     * @param activity The current activity.
     * @param profile The current profile.
     * @param toolbarMenuButton The toolbar menu button to which IPH will be anchored.
     * @param appMenuHandler The app menu handler.
     * @param menuId ID of the menu item to be highlighted.
     */
    public static void maybeShowInProductHelp(
            Activity activity,
            Profile profile,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler,
            int menuId) {
        UserEducationHelper userEducationHelper =
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper()));
        userEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                toolbarMenuButton.getContext().getResources(),
                                FeatureConstants.INSTANCE_SWITCHER,
                                R.string.iph_instance_switcher_text,
                                R.string.iph_instance_switcher_text)
                        .setAnchorView(toolbarMenuButton)
                        .setOnShowCallback(
                                () -> {
                                    appMenuHandler.setMenuHighlight(menuId);
                                })
                        .setOnDismissCallback(
                                () -> {
                                    appMenuHandler.clearMenuHighlight();
                                })
                        .build());
    }
}
