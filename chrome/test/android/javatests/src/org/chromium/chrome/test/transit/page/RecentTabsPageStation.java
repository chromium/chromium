// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.recent_tabs.RecentTabsPage;
import org.chromium.components.embedder_support.util.UrlConstants;

/** A station representing the Recent Tabs page. */
public class RecentTabsPageStation extends CtaPageStation {
    public static final String RECENT_TABS_URL = UrlConstants.RECENT_TABS_URL;
    public final Element<RecentTabsPage> nativePageElement;

    protected RecentTabsPageStation(Config config) {
        super(config);
        declareView(URL_BAR, ViewElement.unscopedOption());
        nativePageElement =
                declareEnterConditionAsElement(
                        new NativePageCondition<>(RecentTabsPage.class, loadedTabElement));
    }

    public static Builder<RecentTabsPageStation> newBuilder() {
        return new Builder<>(RecentTabsPageStation::new);
    }
}
