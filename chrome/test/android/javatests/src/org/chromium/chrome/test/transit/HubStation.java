// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.hub.R;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** The Hub screen, with several panes and a toolbar. */
public class HubStation extends TransitStation {
    // TODO(crbug/1498446): Uncomment once Hub toolbar has a non-zero height.
    // public static final Matcher<View> HUB_TOOLBAR = withId(R.id.hub_toolbar);
    public static final Matcher<View> HUB_PANE_HOST = withId(R.id.hub_pane_host);

    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    /**
     * @params chromeTabbedActivityTestRule The {@link ChromeTabbedActivityTestRule} of the test.
     */
    public HubStation(ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super();
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        // TODO(crbug/1498446): Uncomment once Hub toolbar has a non-zero height.
        // elements.declareView(HUB_TOOLBAR);
        elements.declareView(HUB_PANE_HOST);

        elements.declareEnterCondition(new HubIsEnabled());
        elements.declareEnterCondition(new HubLayoutShowing());
        elements.declareExitCondition(new HubLayoutNotShowing());
    }

    private class HubIsEnabled extends UiThreadCondition {
        @Override
        public boolean check() {
            return HubFieldTrial.isHubEnabled();
        }

        @Override
        public String buildDescription() {
            return "HubFieldTrial Hub is enabled";
        }
    }

    private class HubLayoutShowing extends UiThreadCondition {
        @Override
        public boolean check() {
            LayoutManager layoutManager =
                    mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
            return layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)
                    && !layoutManager.isLayoutStartingToShow(LayoutType.TAB_SWITCHER)
                    && !layoutManager.isLayoutStartingToHide(LayoutType.TAB_SWITCHER);
        }

        @Override
        public String buildDescription() {
            return "LayoutManager is showing TAB_SWITCHER (Hub)";
        }
    }

    private class HubLayoutNotShowing extends UiThreadCondition {
        @Override
        public boolean check() {
            LayoutManager layoutManager =
                    mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
            return !layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER);
        }

        @Override
        public String buildDescription() {
            return "LayoutManager is not showing TAB_SWITCHER (Hub)";
        }
    }
}
