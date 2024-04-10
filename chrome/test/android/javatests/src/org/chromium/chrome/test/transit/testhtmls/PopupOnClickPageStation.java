// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.Trip;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.transit.PopupBlockedMessageFacility;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.HtmlElementInState;
import org.chromium.content_public.browser.test.transit.WebContentsElementInState;

/** PageStation for popup_on_click.html, which contains a link to open itself in a pop-up. */
public class PopupOnClickPageStation extends PageStation {
    public static final String PATH = "/chrome/test/data/android/popup_on_click.html";

    public static final HtmlElement LINK_TO_POPUP = new HtmlElement("link");
    private HtmlElementInState mLinkToPopup;

    protected PopupOnClickPageStation(Builder<PopupOnClickPageStation> builder) {
        super(builder);
    }

    /** Load popup_on_click.html in current tab. */
    public static PopupOnClickPageStation loadInCurrentTab(
            ChromeTabbedActivityTestRule activityTestRule, PageStation currentPageStation) {
        Builder<PopupOnClickPageStation> builder = new Builder<>(PopupOnClickPageStation::new);

        String url = activityTestRule.getTestServer().getURL(PATH);
        return currentPageStation.loadPageProgramatically(builder, url);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        WebContentsElementInState webContents =
                elements.declareElementInState(
                        new WebContentsElementInState(getTestRule()::getWebContents));
        mLinkToPopup =
                elements.declareElementInState(new HtmlElementInState(LINK_TO_POPUP, webContents));
    }

    /** Opens the same page as a pop-up (in Android, this means in a new tab). */
    public PopupOnClickPageStation clickLinkToOpenPopup() {
        PopupOnClickPageStation newPage =
                new Builder<>(PopupOnClickPageStation::new)
                        .initFrom(this)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();
        return Trip.travelSync(this, newPage, Transition.retryOption(), mLinkToPopup::click);
    }

    /**
     * Tries to open same page as a pop-up but expect it to be blocked and for a pop-up blocked
     * message to be shown.
     */
    public PopupBlockedMessageFacility clickLinkAndExpectPopupBlockedMessage() {
        PopupBlockedMessageFacility infoBar = new PopupBlockedMessageFacility(this, 1);
        return StationFacility.enterSync(infoBar, Transition.retryOption(), mLinkToPopup::click);
    }
}
