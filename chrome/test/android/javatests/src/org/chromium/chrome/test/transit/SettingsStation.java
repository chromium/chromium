// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.espresso.Espresso;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.settings.SettingsActivity;

/**
 * The initial and main Settings screen.
 *
 * <p>TODO(crbug.com/328277614): This is a stub; add more elements and methods.
 */
public class SettingsStation extends Station {
    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareActivity(SettingsActivity.class);
        elements.declareView(ViewElement.sharedViewElement(withText("Search engine")));
    }

    /** Press back to leave the SettingsActivity back to the previous state. */
    public <T extends Station> T pressBack(T station) {
        return Trip.travelSync(this, station, () -> Espresso.pressBack());
    }
}
