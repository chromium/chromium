// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

/** Controller to manage when and how we show multi-instance in-product help messages to users. */
@NullMarked
public class MultiInstanceIphController {
    /**
     * Attempts to show an IPH text bubble about multi-instance features accessible from the app
     * menu.
     *
     * @param activity The current activity.
     * @param profile The current profile.
     * @param toolbarMenuButton The toolbar menu button to which IPH will be anchored.
     * @param appMenuHandler The app menu handler.
     * @param featureName Name of the Feature Engagement feature.
     * @param stringId String resource ID of the IPH bubble text.
     * @param menuId ID of the menu item to be highlighted.
     */
    public static void maybeShowInProductHelp(
            Activity activity,
            Profile profile,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler,
            String featureName,
            int stringId,
            int menuId) {
        UserEducationHelper userEducationHelper =
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper()));
        userEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                toolbarMenuButton.getContext().getResources(),
                                featureName,
                                stringId,
                                stringId)
                        .setAnchorView(toolbarMenuButton)
                        .setOnShowCallback(
                                () -> {
                                    appMenuHandler.setMenuHighlight(
                                            menuId,
                                            /* shouldHighlightMenuButton= */ true,
                                            /* shouldScroll= */ true);
                                })
                        .setOnDismissCallback(
                                () -> {
                                    appMenuHandler.clearMenuHighlight();
                                })
                        .build());
    }
}
