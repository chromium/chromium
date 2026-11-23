// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.browser_controls.BrowserControlsFacility;
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
    private static final HtmlElementSpec TOP_LINK = new HtmlElementSpec("#top_link");
    private static final HtmlElementSpec BOTTOM_LINK = new HtmlElementSpec("#bottom_link");

    /** The top of the page, entered by default when the page loads. */
    public final TopFacility topFacility;

    protected TopBottomLinksPageStation(Config config) {
        super(config);
        topFacility = declareDefaultFacility(new TopFacility());
    }

    /** Load the page, landing at its {@link TopFacility} by default. */
    public static TopBottomLinksPageStation loadPage(
            ChromeTabbedActivityTestRule activityTestRule, CtaPageStation currentPageStation) {
        String url = activityTestRule.getTestServer().getURL(PATH);
        TopBottomLinksPageStation station =
                new Builder<>(TopBottomLinksPageStation::new)
                        .initForLoadingUrlOnSameTab(url, currentPageStation)
                        .build();
        return currentPageStation.loadUrlTo(url).arriveAt(station);
    }

    /** The page is scrolled to the top, and the top link is displayed. */
    public static class TopFacility extends Facility<TopBottomLinksPageStation> {
        public HtmlElement topElement;

        @Override
        public void declareExtraElements() {
            topElement =
                    declareElement(
                            new HtmlElement(
                                    TOP_LINK,
                                    mHostStation.webContentsElement,
                                    HtmlElement.inViewportOption()));
            declareEnterCondition(new ScrollToTopCondition(mHostStation.webContentsElement));
        }

        /** Open context menu on the top link. */
        public LinkContextMenuFacility openContextMenuOnTopLink() {
            return topElement.longPressTo().enterFacility(new LinkContextMenuFacility());
        }

        /** Scroll to the bottom of the page. */
        public BottomFacility scrollToBottom() {
            return scrollToBottom(/* browserControlsState= */ null);
        }

        /** Scroll to the bottom of the page and assert browser controls state. */
        public BottomFacility scrollToBottom(
                @Nullable BrowserControlsFacility<TopBottomLinksPageStation> browserControlsState) {
            BottomFacility bottomFacility = new BottomFacility();
            TripBuilder builder =
                    mHostStation.scrollPageDownWithGestureTo().withRetry().exitFacilityAnd(this);
            if (browserControlsState != null) {
                builder.enterFacilities(browserControlsState, bottomFacility);
            } else {
                builder.enterFacility(bottomFacility);
            }
            return bottomFacility;
        }
    }

    /** The page is scrolled to the bottom, and the bottom link is displayed. */
    public static class BottomFacility extends Facility<TopBottomLinksPageStation> {
        public HtmlElement bottomElement;

        @Override
        public void declareExtraElements() {
            bottomElement =
                    declareElement(
                            new HtmlElement(
                                    BOTTOM_LINK,
                                    mHostStation.webContentsElement,
                                    HtmlElement.inViewportOption()));
            declareEnterCondition(new ScrollToBottomCondition(mHostStation.webContentsElement));
        }

        /** Open context menu on the bottom link. */
        public LinkContextMenuFacility openContextMenuOnBottomLink() {
            return bottomElement.longPressTo().enterFacility(new LinkContextMenuFacility());
        }

        /** Scroll to the top of the page. */
        public TopFacility scrollToTop() {
            return scrollToTop(/* browserControlsState= */ null);
        }

        /** Scroll to the top of the page and assert browser controls state. */
        public TopFacility scrollToTop(
                @Nullable BrowserControlsFacility<TopBottomLinksPageStation> browserControlsState) {
            TopFacility topFacility = new TopFacility();
            TripBuilder builder =
                    mHostStation.scrollPageUpWithGestureTo().withRetry().exitFacilityAnd(this);
            if (browserControlsState != null) {
                builder.enterFacilities(browserControlsState, topFacility);
            } else {
                builder.enterFacility(topFacility);
            }
            return topFacility;
        }
    }
}
