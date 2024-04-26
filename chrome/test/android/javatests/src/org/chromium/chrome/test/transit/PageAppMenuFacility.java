// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.Elements;

/** The app menu shown when pressing ("...") in a Tab. */
public class PageAppMenuFacility extends AppMenuFacility<PageStation> {

    public PageAppMenuFacility(PageStation station) {
        super(station, station.mChromeTabbedActivityTestRule);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        // Just wait for the first items because of scrolling.
        elements.declareView(NEW_TAB_ITEM);
        elements.declareView(NEW_INCOGNITO_TAB_ITEM);
    }
}
