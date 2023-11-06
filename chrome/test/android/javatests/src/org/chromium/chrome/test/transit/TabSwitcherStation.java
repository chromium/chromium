// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.either;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ToolbarTestUtils;

/** The tab switcher screen, with the tab grid and the tab management toolbar. */
public class TabSwitcherStation extends TransitStation {
    public static final Matcher<View> TOOLBAR = withId(ToolbarTestUtils.TAB_SWITCHER_TOOLBAR);
    public static final Matcher<View> TOOLBAR_NEW_TAB_BUTTON =
            either(withId(ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_NEW_TAB))
                    .or(withId(ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION));

    public static final Matcher<View> INCOGNITO_TOGGLE_TABS = withId(R.id.incognito_toggle_tabs);
    public static final Matcher<View> EMPTY_STATE_TEXT =
            withText(R.string.tabswitcher_no_tabs_empty_state);

    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    public TabSwitcherStation(ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super();
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(TOOLBAR);
        elements.declareView(TOOLBAR_NEW_TAB_BUTTON);

        TabModelSelector tabModelSelector =
                mChromeTabbedActivityTestRule.getActivity().getTabModelSelector();
        if (tabModelSelector.isIncognitoSelected()) {
            elements.declareView(INCOGNITO_TOGGLE_TABS);
        } else {
            if (tabModelSelector.getModel(false).getCount() == 0) {
                elements.declareView(EMPTY_STATE_TEXT);
            }
        }

        elements.declareEnterCondition(new HubIsDisabled());
        elements.declareEnterCondition(new TabSwitcherLayoutShowing());
        elements.declareExitCondition(new TabSwitcherLayoutNotShowing());
    }

    private class HubIsDisabled extends UiThreadCondition {
        @Override
        public boolean check() {
            return !HubFieldTrial.isHubEnabled();
        }

        @Override
        public String buildDescription() {
            return "HubFieldTrial Hub is disabled";
        }
    }

    private class TabSwitcherLayoutShowing extends UiThreadCondition {
        @Override
        public boolean check() {
            LayoutManager layoutManager =
                    mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
            // TODO: Use #isLayoutFinishedShowing(LayoutType.TAB_SWITCHER) once available.
            return layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER);
        }

        @Override
        public String buildDescription() {
            return "LayoutManager is showing TAB_SWITCHER";
        }
    }

    private class TabSwitcherLayoutNotShowing extends UiThreadCondition {
        @Override
        public boolean check() {
            LayoutManager layoutManager =
                    mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
            // TODO: Use #isLayoutHidden(LayoutType.TAB_SWITCHER) once available.
            return !layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER);
        }

        @Override
        public String buildDescription() {
            return "LayoutManager is not showing TAB_SWITCHER";
        }
    }
}
