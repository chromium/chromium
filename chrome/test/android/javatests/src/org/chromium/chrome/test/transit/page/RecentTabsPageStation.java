// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;

/** A station representing the Recent Tabs page. */
public class RecentTabsPageStation extends CtaPageStation {
    protected RecentTabsPageStation(Config config) {
        super(config);
        declareView(URL_BAR, ViewElement.unscopedOption());
        declareView(withText(R.string.recently_closed));
    }

    public static Builder<RecentTabsPageStation> newBuilder() {
        return new Builder<>(RecentTabsPageStation::new);
    }
}
