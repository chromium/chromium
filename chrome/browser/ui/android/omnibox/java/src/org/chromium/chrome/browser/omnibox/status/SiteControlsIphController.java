// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * Controller to manage the In-Product Help (IPH) for Site Controls when it moves to the app menu.
 */
@NullMarked
public class SiteControlsIphController {
    private final UserEducationHelper mUserEducationHelper;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;

    /**
     * Constructor for {@link SiteControlsIphController}.
     *
     * @param userEducationHelper The helper to show IPH.
     * @param toolbarMenuButton The toolbar menu button to which IPH will be anchored.
     * @param appMenuHandler The app menu handler used to highlight the menu item.
     */
    public SiteControlsIphController(
            UserEducationHelper userEducationHelper,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler) {
        mUserEducationHelper = userEducationHelper;
        mToolbarMenuButton = toolbarMenuButton;
        mAppMenuHandler = appMenuHandler;
    }

    /**
     * Shows the IPH bubble pointing to the app menu and prepares the highlight for the "Site
     * controls" item.
     */
    public void showIph() {
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mToolbarMenuButton.getResources(),
                                FeatureConstants.SITE_CONTROLS_FEATURE,
                                R.string.iph_site_controls_text,
                                R.string.iph_site_controls_text)
                        .setAnchorView(mToolbarMenuButton)
                        .setOnShowCallback(
                                () -> mAppMenuHandler.setMenuHighlight(R.id.info_menu_id))
                        .setOnDismissCallback(mAppMenuHandler::clearMenuHighlight)
                        .build());
    }
}
