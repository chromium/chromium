// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import android.util.Pair;

import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.transit.PopupBlockedMessageFacility;

/**
 * PageStation for popup_test.html, which opens two pop-ups (one.html and two.html) upon loading.
 */
public class PopupOnLoadPageStation extends PageStation {
    public static final String PATH = "/chrome/test/data/android/popup_test.html";

    protected PopupOnLoadPageStation(Builder<PopupOnLoadPageStation> builder) {
        super(builder);
    }

    /**
     * Load popup_test.html in current tab and expect the pop-up to be blocked and a pop-up blocked
     * message to be displayed.
     *
     * @return the now active PopupOnLoadPageStation and the entered PopupBlockedMessageFacility
     */
    public static Pair<PopupOnLoadPageStation, PopupBlockedMessageFacility>
            loadInCurrentTabExpectBlocked(
                    ChromeTabbedActivityTestRule activityTestRule, PageStation currentPageStation) {
        PopupOnLoadPageStation newPage =
                new Builder<>(PopupOnLoadPageStation::new)
                        .initFrom(currentPageStation)
                        .withIsOpeningTab(false)
                        .withIsSelectingTab(false)
                        .build();
        PopupBlockedMessageFacility popupBlockedMessage =
                new PopupBlockedMessageFacility(newPage, 2);
        // TODO(crbug.com/329307093): Add condition that no new tabs were opened.

        String url = activityTestRule.getTestServer().getURL(PATH);
        currentPageStation.loadPageProgramatically(newPage, url);

        return Pair.create(newPage, popupBlockedMessage);
    }

    /**
     * Load popup_test.html in current tab and expect two pop-ups to be opened.
     *
     * @return the now active PageStation two.html
     */
    public static PageStation loadInCurrentTabExpectPopups(
            ChromeTabbedActivityTestRule activityTestRule, PageStation currentPageStation) {
        // Don't expect popup_test.html to be loaded at the end of the transition, but two.html.
        PageStation newPage =
                NavigatePageStations.newNavigateTwoPageBuilder()
                        .initFrom(currentPageStation)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();
        // TODO(crbug.com/329307093): Add condition that two new tabs were opened.

        String url = activityTestRule.getTestServer().getURL(PATH);
        return currentPageStation.loadPageProgramatically(newPage, url);
    }
}
