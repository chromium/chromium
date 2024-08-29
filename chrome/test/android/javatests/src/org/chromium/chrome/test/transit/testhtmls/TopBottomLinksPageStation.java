// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import android.view.View;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Elements;
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
 * links does not show up the same time on screen.
 */
public class TopBottomLinksPageStation extends WebPageStation {
    private static final String PATH =
            "/chrome/test/data/android/edge_to_edge/top-bottom-links.html";
    private static final HtmlElementSpec TOP_LINK = new HtmlElementSpec("top_link");
    private static final HtmlElementSpec BOTTOM_LINK = new HtmlElementSpec("bottom_link");

    protected <T extends TopBottomLinksPageStation> TopBottomLinksPageStation(Builder<T> builder) {
        super(builder);
    }

    protected HtmlElement mTopElement;
    protected HtmlElement mBottomElement;

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        mTopElement = elements.declareElement(new HtmlElement(TOP_LINK, mWebContentsSupplier));
        mBottomElement =
                elements.declareElement(new HtmlElement(BOTTOM_LINK, mWebContentsSupplier));
    }

    /** Load the page and lands on a {@link TopBottomLinksPageStation}. */
    public static TopBottomLinksPageStation loadPage(
            ChromeTabbedActivityTestRule activityTestRule, PageStation currentPageStation) {
        String url = activityTestRule.getTestServer().getURL(PATH);
        return currentPageStation.loadPageProgrammatically(
                url, new Builder<TopBottomLinksPageStation>(TopBottomLinksPageStation::new));
    }

    // TODO(crbug.com/362995902): Make this more generic and move to WebPageStation.
    /** Scroll to the bottom of the page. */
    public TopBottomLinksPageStation scrollToBottom() {
        Condition.runAndWaitFor(
                this::scrollDown, new ScrollToBottomCondition(mWebContentsSupplier));
        return this;
    }

    /** Open context menu on the top link. */
    public LinkContextMenuFacility openContextMenuOnTopLink() {
        return enterFacilitySync(new LinkContextMenuFacility(), mTopElement::longPress);
    }

    /** Open context menu on the bottom link. */
    public LinkContextMenuFacility openContextMenuOnBottomLink() {
        return enterFacilitySync(new LinkContextMenuFacility(), mBottomElement::longPress);
    }

    private void scrollDown() {
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
    }

    private void scrollUp() {
        View contentView = mActivityTabSupplier.get().getView();
        float width = contentView.getWidth();
        float height = contentView.getHeight();
        float fromY = height - height / 10;
        float toY = height / 10;
        TouchCommon.performDragNoFling(
                mActivityElement.get(),
                width / 2,
                width / 2,
                fromY,
                toY,
                /* steps= */ 50,
                /* duration= */ 500);
    }
}
