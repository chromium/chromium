// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.page.CctPageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
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

    protected PopupOnClickPageStation(Config config) {
        super(config);

        linkToPopup =
                declareElement(new HtmlElement(new HtmlElementSpec("link"), webContentsElement));
        linkToPopupWithBounds =
                declareElement(
                        new HtmlElement(
                                new HtmlElementSpec("link_with_bounds"), webContentsElement));
    }

    /** Load popup_on_click.html in current tab. */
    public static PopupOnClickPageStation loadInCurrentTab(
            ChromeTabbedActivityTestRule activityTestRule, CtaPageStation currentPageStation) {
        Builder<PopupOnClickPageStation> builder = new Builder<>(PopupOnClickPageStation::new);

        String url = activityTestRule.getTestServer().getURL(PATH);
        return currentPageStation.loadPageProgrammatically(url, builder);
    }

    /** Opens the same page as a pop-up (in Android, this means in a new tab). */
    public PopupOnClickPageStation clickLinkToOpenPopup() {
        PopupOnClickPageStation newPage =
                new Builder<>(PopupOnClickPageStation::new)
                        .initFrom(this)
                        .initOpeningNewTab()
                        .build();
        return linkToPopup.clickTo().arriveAt(newPage);
    }

    /** Opens a sample page as a pop-up with bounds and expects a new window to open. */
    public CctPageStation clickLinkToOpenPopupWithBoundsExpectNewWindow() {
        return linkToPopupWithBounds
                .clickTo()
                .inNewTask()
                .arriveAt(
                        CctPageStation.newBuilder()
                                .withEntryPoint()
                                .withExpectedUrlSubstring("simple.html")
                                .withExpectedTitle("Simple")
                                .withIncognito(mIsIncognito)
                                .build());
    }

    /**
     * Tries to open same page as a pop-up but expect it to be blocked and for a pop-up blocked
     * message to be shown.
     */
    public PopupBlockedMessageFacility clickLinkAndExpectPopupBlockedMessage() {
        return linkToPopup.clickTo().enterFacility(new PopupBlockedMessageFacility<>(1));
    }
}
