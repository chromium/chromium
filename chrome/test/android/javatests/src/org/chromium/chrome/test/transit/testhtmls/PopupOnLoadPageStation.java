// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import android.util.Pair;

import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.PopupBlockedMessageFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;

/**
 * PageStation for popup_test.html, which opens two pop-ups (one.html and two.html) upon loading.
 */
public class PopupOnLoadPageStation extends WebPageStation {
    public static final String PATH = "/chrome/test/data/android/popup_test.html";

    protected <T extends PopupOnLoadPageStation> PopupOnLoadPageStation(Builder<T> builder) {
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
        // TODO(crbug.com/329307093): Add condition that no new tabs were opened.
        String url = activityTestRule.getTestServer().getURL(PATH);
        PopupBlockedMessageFacility<PopupOnLoadPageStation> popupBlockedMessage =
                new PopupBlockedMessageFacility<>(2);
        PopupOnLoadPageStation newPage =
                currentPageStation.loadPageProgrammatically(
                        url,
                        new Builder<PopupOnLoadPageStation>(PopupOnLoadPageStation::new)
                                .withFacility(popupBlockedMessage));

        return Pair.create(newPage, popupBlockedMessage);
    }

    /**
     * Load popup_test.html in current tab and expect two pop-ups to be opened.
     *
     * @return the now active PageStation two.html
     */
    public static WebPageStation loadInCurrentTabExpectPopups(
            ChromeTabbedActivityTestRule activityTestRule, PageStation currentPageStation) {
        // TODO(crbug.com/329307093): Add condition that two new tabs were opened.
        String url = activityTestRule.getTestServer().getURL(PATH);
        return currentPageStation.loadPageProgrammatically(
                url,
                NavigatePageStations.newNavigateTwoPageBuilder()
                        .withIsOpeningTabs(2)
                        .withIsSelectingTabs(2)
                        // Expect popup_test.html to open two.html in foreground.
                        .withExpectedUrlSubstring("two.html"));
    }
}
