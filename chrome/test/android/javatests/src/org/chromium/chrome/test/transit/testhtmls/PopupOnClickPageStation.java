// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.page.CctPageStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.PopupBlockedMessageFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.HtmlElementSpec;

/**
 * PageStation for popup_on_click.html, which contains links to open pop-ups with different
 * parameters.
 */
public class PopupOnClickPageStation extends WebPageStation {
    public static final String PATH = "/chrome/test/data/android/popup_on_click.html";

    public HtmlElement linkToPopup;
    public HtmlElement linkToPopupWithBounds;

    protected <T extends PopupOnClickPageStation> PopupOnClickPageStation(Builder<T> builder) {
        super(builder);

        linkToPopup =
                declareElement(new HtmlElement(new HtmlElementSpec("link"), webContentsElement));
        linkToPopupWithBounds =
                declareElement(
                        new HtmlElement(
                                new HtmlElementSpec("link_with_bounds"), webContentsElement));
    }

    /** Load popup_on_click.html in current tab. */
    public static PopupOnClickPageStation loadInCurrentTab(
            ChromeTabbedActivityTestRule activityTestRule, PageStation currentPageStation) {
        Builder<PopupOnClickPageStation> builder = new Builder<>(PopupOnClickPageStation::new);

        String url = activityTestRule.getTestServer().getURL(PATH);
        return currentPageStation.loadPageProgrammatically(url, builder);
    }

    /** Opens the same page as a pop-up (in Android, this means in a new tab). */
    public PopupOnClickPageStation clickLinkToOpenPopup() {
        PopupOnClickPageStation newPage =
                new Builder<PopupOnClickPageStation>(PopupOnClickPageStation::new)
                        .initFrom(this)
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return travelToSync(newPage, linkToPopup.getClickTrigger());
    }

    /** Opens a sample page as a pop-up with bounds and expects a new window to open. */
    public CctPageStation clickLinkToOpenPopupWithBoundsExpectNewWindow() {
        CctPageStation newPage =
                CctPageStation.newBuilder()
                        .withEntryPoint()
                        .withExpectedUrlSubstring("simple.html")
                        .withExpectedTitle("Simple")
                        .build();
        return spawnSync(newPage, linkToPopupWithBounds.getClickTrigger());
    }

    /**
     * Tries to open same page as a pop-up but expect it to be blocked and for a pop-up blocked
     * message to be shown.
     */
    public PopupBlockedMessageFacility clickLinkAndExpectPopupBlockedMessage() {
        return enterFacilitySync(
                new PopupBlockedMessageFacility<PopupOnClickPageStation>(1),
                linkToPopup.getClickTrigger());
    }
}
