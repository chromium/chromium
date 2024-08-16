// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import org.chromium.base.test.transit.Elements;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.PopupBlockedMessageFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.HtmlElementSpec;

/** PageStation for popup_on_click.html, which contains a link to open itself in a pop-up. */
public class PopupOnClickPageStation extends WebPageStation {
    public static final String PATH = "/chrome/test/data/android/popup_on_click.html";

    public static final HtmlElementSpec LINK_TO_POPUP = new HtmlElementSpec("link");
    private HtmlElement mLinkToPopup;

    protected <T extends PopupOnClickPageStation> PopupOnClickPageStation(Builder<T> builder) {
        super(builder);
    }

    /** Load popup_on_click.html in current tab. */
    public static PopupOnClickPageStation loadInCurrentTab(
            ChromeTabbedActivityTestRule activityTestRule, PageStation currentPageStation) {
        Builder<PopupOnClickPageStation> builder = new Builder<>(PopupOnClickPageStation::new);

        String url = activityTestRule.getTestServer().getURL(PATH);
        return currentPageStation.loadPageProgrammatically(url, builder);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        mLinkToPopup =
                elements.declareElement(new HtmlElement(LINK_TO_POPUP, mWebContentsSupplier));
    }

    /** Opens the same page as a pop-up (in Android, this means in a new tab). */
    public PopupOnClickPageStation clickLinkToOpenPopup() {
        PopupOnClickPageStation newPage =
                new Builder<PopupOnClickPageStation>(PopupOnClickPageStation::new)
                        .initFrom(this)
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return travelToSync(newPage, mLinkToPopup::click);
    }

    /**
     * Tries to open same page as a pop-up but expect it to be blocked and for a pop-up blocked
     * message to be shown.
     */
    public PopupBlockedMessageFacility clickLinkAndExpectPopupBlockedMessage() {
        return enterFacilitySync(
                new PopupBlockedMessageFacility<PopupOnClickPageStation>(1), mLinkToPopup::click);
    }
}
