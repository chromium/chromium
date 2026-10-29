// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import static org.chromium.base.test.transit.Triggers.noopTo;
import static org.chromium.base.test.transit.Triggers.runOnUiThreadTo;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.TrafficControl;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherStation;
import org.chromium.chrome.test.transit.layouts.LayoutTypeVisibleCondition;
import org.chromium.chrome.test.transit.page.CtaPageStation;

/** Utilities to aid in testing that involves switching between layouts. */
public class LayoutTestUtils {
    /**
     * Waits for the {@link LayoutType#HUB} layout to be shown and hops on Public Transit at the
     * {@link TabSwitcherStation} for the current tab model.
     *
     * @param cta The {@link ChromeTabbedActivity} showing the layout.
     * @return the {@link TabSwitcherStation} hopped on at.
     */
    public static TabSwitcherStation waitForHubLayout(ChromeTabbedActivity cta) {
        TabModelSelector selector = ThreadUtils.runOnUiThreadBlocking(cta::getTabModelSelector);
        boolean incognito = ThreadUtils.runOnUiThreadBlocking(selector::isIncognitoSelected);
        TabSwitcherStation tabSwitcher =
                incognito
                        ? IncognitoTabSwitcherStation.from(selector)
                        : RegularTabSwitcherStation.from(selector);
        return TrafficControl.hopOnAt(cta, tabSwitcher);
    }

    /**
     * Waits for the {@link LayoutType#BROWSING} layout to be shown and hops on Public Transit at a
     * {@link CtaPageStation} for the activity tab.
     *
     * <p>The Station also waits for the page and the toolbar views, so use {@link
     * #waitForLayout(LayoutManager, int)} instead when the Omnibox may be focused.
     *
     * @param cta The {@link ChromeTabbedActivity} showing the layout.
     * @return the {@link CtaPageStation} hopped on at.
     */
    public static CtaPageStation waitForBrowsingLayout(ChromeTabbedActivity cta) {
        waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        Tab tab = ThreadUtils.runOnUiThreadBlocking(cta::getActivityTab);
        return TrafficControl.hopOnAt(
                cta,
                CtaPageStation.newGenericBuilder()
                        .withTabAlreadySelected(tab)
                        .withIncognito(tab.isIncognitoBranded())
                        .build());
    }

    /**
     * Wait for a specified layout to be shown. If the layout is already showing, this returns
     * immediately.
     *
     * <p>Prefer {@link #waitForHubLayout(ChromeTabbedActivity)} or {@link
     * #waitForBrowsingLayout(ChromeTabbedActivity)}, which also hop on Public Transit.
     *
     * @param layoutManager The {@link LayoutManager} showing the layout.
     * @param type The type of layout to wait for.
     */
    public static void waitForLayout(LayoutManager layoutManager, @LayoutType int type) {
        Assert.assertNotNull(layoutManager);
        noopTo().waitFor(new LayoutTypeVisibleCondition(layoutManager, type));
    }

    /**
     * Start showing a layout and wait for it to finish showing.
     *
     * @param layoutManager A layout manager to show different layouts.
     * @param type The type of layout to show.
     * @param animate Whether to animate the transition.
     */
    public static void startShowingAndWaitForLayout(
            LayoutManager layoutManager, @LayoutType int type, boolean animate) {
        runOnUiThreadTo(() -> layoutManager.showLayout(type, animate))
                .withPossiblyAlreadyFulfilled()
                .waitFor(new LayoutTypeVisibleCondition(layoutManager, type));
    }
}
