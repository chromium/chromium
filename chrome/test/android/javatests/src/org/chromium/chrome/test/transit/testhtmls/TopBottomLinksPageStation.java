// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import android.util.Pair;

import org.chromium.base.test.transit.Facility;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.context_menu.LinkContextMenuFacility;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.HtmlElementSpec;

/**
 * Station that has a top and bottom page. The test page contains a link on the top, and one at the
 * bottom. The links are separated with 1.5x the size of viewport, so it's guaranteed these two
 * links do not show up the same time on screen.
 */
public class TopBottomLinksPageStation extends WebPageStation {
    private static final String PATH =
            "/chrome/test/data/android/edge_to_edge/top-bottom-links.html";
    private static final HtmlElementSpec TOP_LINK = new HtmlElementSpec("top_link");
    private static final HtmlElementSpec BOTTOM_LINK = new HtmlElementSpec("bottom_link");

    protected TopBottomLinksPageStation(Config config) {
        super(config);
    }

    /** Load the page, land at the {@link TopFacility} of a {@link TopBottomLinksPageStation}. */
    public static Pair<TopBottomLinksPageStation, TopFacility> loadPage(
            ChromeTabbedActivityTestRule activityTestRule, CtaPageStation currentPageStation) {
        String url = activityTestRule.getTestServer().getURL(PATH);
        TopFacility topFacility = new TopFacility();
        TopBottomLinksPageStation station =
                new Builder<>(TopBottomLinksPageStation::new)
                        .initForLoadingUrlOnSameTab(url, currentPageStation)
                        .build();

        currentPageStation.loadUrlTo(url).arriveAt(station, topFacility);
        return Pair.create(station, topFacility);
    }

    /** The page is scrolled to the top, and the top link is displayed. */
    public static class TopFacility extends Facility<TopBottomLinksPageStation> {
        public HtmlElement topElement;

        @Override
        public void declareExtraElements() {
            topElement = declareElement(new HtmlElement(TOP_LINK, mHostStation.webContentsElement));
        }

        /** Open context menu on the top link. */
        public LinkContextMenuFacility openContextMenuOnTopLink() {
            return topElement.longPressTo().enterFacility(new LinkContextMenuFacility());
        }

        /** Scroll to the bottom of the page. */
        public BottomFacility scrollToBottom() {
            return mHostStation
                    .scrollPageDownWithGestureTo()
                    .withRetry()
                    .exitFacilityAnd(this)
                    .enterFacility(new BottomFacility());
        }
    }

    /** The page is scrolled to the bottom, and the bottom link is displayed. */
    public static class BottomFacility extends Facility<TopBottomLinksPageStation> {
        public HtmlElement bottomElement;

        @Override
        public void declareExtraElements() {
            bottomElement =
                    declareElement(new HtmlElement(BOTTOM_LINK, mHostStation.webContentsElement));
            declareEnterCondition(new ScrollToBottomCondition(mHostStation.webContentsElement));
        }

        /** Open context menu on the bottom link. */
        public LinkContextMenuFacility openContextMenuOnBottomLink() {
            return bottomElement.longPressTo().enterFacility(new LinkContextMenuFacility());
        }

        /** Scroll to the bottom of the page. */
        public TopFacility scrollToTop() {
            return mHostStation
                    .scrollPageUpWithGestureTo()
                    .withRetry()
                    .exitFacilityAnd(this)
                    .enterFacility(new TopFacility());
        }
    }
}
