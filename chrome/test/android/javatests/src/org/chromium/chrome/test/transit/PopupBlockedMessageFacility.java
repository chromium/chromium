// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.R;

public class PopupBlockedMessageFacility extends StationFacility<PageStation> {

    public static final ViewElement ICON = sharedViewElement(withId(R.id.message_icon));
    public static final ViewElement ALWAYS_SHOW_BUTTON = sharedViewElement(withText("Always show"));

    private final int mCount;

    public PopupBlockedMessageFacility(PageStation station, int count) {
        super(station);
        mCount = count;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(ICON);
        if (mCount == 1) {
            elements.declareView(sharedViewElement(withText("Pop-up blocked")));
        } else {
            elements.declareView(
                    sharedViewElement(withText(String.format("%s pop-ups blocked", mCount))));
        }
        elements.declareView(ALWAYS_SHOW_BUTTON);
    }

    public PageStation clickAlwaysAllow() {
        PageStation popupPage =
                PageStation.newPageStationBuilder()
                        .initFrom(mStation)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();
        return Trip.travelSync(mStation, popupPage, () -> ALWAYS_SHOW_BUTTON.perform(click()));
    }
}
