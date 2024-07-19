// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static androidx.test.espresso.action.ViewActions.click;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.transit.MessageFacility;

/** Represents a "Pop-up blocked" message. */
public class PopupBlockedMessageFacility<HostStationT extends WebPageStation>
        extends MessageFacility<HostStationT> {

    public static final ViewElement ALWAYS_SHOW_BUTTON = primaryButtonViewElement("Always show");

    private final int mCount;

    public PopupBlockedMessageFacility(int count) {
        mCount = count;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        String title;
        if (mCount == 1) {
            title = "Pop-up blocked";
        } else {
            title = String.format("%s pop-ups blocked", mCount);
        }
        elements.declareView(titleViewElement(title));

        elements.declareView(ALWAYS_SHOW_BUTTON);
    }

    public WebPageStation clickAlwaysAllow() {
        WebPageStation popupPage =
                WebPageStation.newWebPageStationBuilder()
                        .initFrom(mHostStation)
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(popupPage, () -> ALWAYS_SHOW_BUTTON.perform(click()));
    }
}
