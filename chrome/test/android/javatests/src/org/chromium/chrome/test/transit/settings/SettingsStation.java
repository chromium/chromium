// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.settings.SettingsActivity;

/**
 * The initial and main Settings screen.
 *
 * <p>TODO(crbug.com/328277614): This is a stub; add more elements and methods.
 */
public class SettingsStation extends Station {

    public static final ViewSpec SEARCH_ENGINE = viewSpec(withText("Search engine"));

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareActivity(SettingsActivity.class);
        elements.declareView(SEARCH_ENGINE);
    }
}
