// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import android.util.Pair;
import android.view.View;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Transition;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.context_menu.LinkContextMenuFacility;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.HtmlElementSpec;
import org.chromium.content_public.browser.test.util.TouchCommon;

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

    protected <T extends TopBottomLinksPageStation> TopBottomLinksPageStation(Builder<T> builder) {
        super(builder);
    }

    /** Load the page, land at the {@link TopFacility} of a {@link TopBottomLinksPageStation}. */
    public static Pair<TopBottomLinksPageStation, TopFacility> loadPage(
            ChromeTabbedActivityTestRule activityTestRule, PageStation currentPageStation) {
        String url = activityTestRule.getTestServer().getURL(PATH);
        TopFacility topFacility = new TopFacility();
        TopBottomLinksPageStation station =
                currentPageStation.loadPageProgrammatically(
                        url,
                        new Builder<TopBottomLinksPageStation>(TopBottomLinksPageStation::new)
                                .withFacility(topFacility));
        return Pair.create(station, topFacility);
    }

    /** Scrolls down the page using a drag gesture to dismiss browser controls. */
    private Transition.Trigger gestureScrollToBottomTrigger() {
        return () -> {
            assertSuppliersCanBeUsed();
            View contentView = mActivityTabSupplier.get().getView();
            float width = contentView.getWidth();
            float height = contentView.getHeight();
            // Start the scroll with some height to avoid touching the nav bar region.
            float fromY = height - height / 10;
            float toY = 0;
            TouchCommon.performDragNoFling(
                    mActivityElement.get(),
                    width / 2,
                    width / 2,
                    fromY,
                    toY,
                    /* steps= */ 50,
                    /* duration= */ 500);
        };
    }

    /** The page is scrolled to the top, and the top link is displayed. */
    public static class TopFacility extends Facility<TopBottomLinksPageStation> {
        protected HtmlElement mTopElement;

        @Override
        public void declareElements(Elements.Builder elements) {
            mTopElement =
                    elements.declareElement(
                            new HtmlElement(TOP_LINK, mHostStation.mWebContentsSupplier));
        }

        /** Open context menu on the top link. */
        public LinkContextMenuFacility openContextMenuOnTopLink() {
            return mHostStation.enterFacilitySync(
                    new LinkContextMenuFacility(), mTopElement::longPress);
        }

        /** Scroll to the bottom of the page. */
        public BottomFacility scrollToBottom() {
            return mHostStation.swapFacilitySync(
                    this, new BottomFacility(), mHostStation.gestureScrollToBottomTrigger());
        }
    }

    /** The page is scrolled to the bottom, and the bottom link is displayed. */
    public static class BottomFacility extends Facility<TopBottomLinksPageStation> {
        protected HtmlElement mBottomElement;

        @Override
        public void declareElements(Elements.Builder elements) {
            mBottomElement =
                    elements.declareElement(
                            new HtmlElement(BOTTOM_LINK, mHostStation.mWebContentsSupplier));
            elements.declareEnterCondition(
                    new ScrollToBottomCondition(mHostStation.mWebContentsSupplier));
        }

        /** Open context menu on the bottom link. */
        public LinkContextMenuFacility openContextMenuOnBottomLink() {
            return mHostStation.enterFacilitySync(
                    new LinkContextMenuFacility(), mBottomElement::longPress);
        }
    }
}
