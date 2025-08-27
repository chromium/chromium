// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertNotNull;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubUtils;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.components.omnibox.OmniboxFeatures;

/** The tab groups pane station. */
public class TabGroupPaneStation extends HubBaseStation {
    public ViewElement<RecyclerView> recyclerViewElement;
    public @Nullable ViewElement<View> newTabGroupButtonElement;
    public ViewElement<View> searchElement;

    public TabGroupPaneStation(boolean regularTabsExist, boolean incognitoTabsExist) {
        super(
                /* isIncognito= */ false,
                regularTabsExist,
                incognitoTabsExist,
                /* hasMenuButton= */ false);

        // TODO(crbug.com/413652567): Handle empty state case.
        recyclerViewElement =
                declareView(
                        paneHostElement.descendant(
                                RecyclerView.class, withId(R.id.tab_group_list_recycler_view)));

        if (ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled()) {
            newTabGroupButtonElement =
                    declareView(toolbarElement.descendant(withId(R.id.toolbar_action_button)));
        }

        if (OmniboxFeatures.sAndroidHubSearchTabGroups.isEnabled()
                && OmniboxFeatures.sAndroidHubSearchEnableOnTabGroupsPane.getValue()) {
            declareElementFactory(
                    mActivityElement,
                    delayedElements -> {
                        Matcher<View> searchBox = withId(R.id.search_box);
                        ViewSpec<View> searchLoupe =
                                toolbarElement.descendant(withId(R.id.search_loupe));
                        if (shouldHubSearchBoxBeVisible()) {
                            searchElement = delayedElements.declareView(searchLoupe);
                            delayedElements.declareNoView(searchBox);
                        } else {
                            searchElement = delayedElements.declareView(searchBox);
                            delayedElements.declareNoView(searchLoupe);
                        }
                    });
        }
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.TAB_GROUPS;
    }

    public NewTabGroupDialogFacility<TabGroupPaneStation> createNewTabGroup() {
        assertNotNull(newTabGroupButtonElement);
        return Journeys.beginNewTabGroupUiFlow(newTabGroupButtonElement.clickTo());
    }

    public TabSwitcherSearchStation openTabGroupsPaneSearch() {
        TabSwitcherSearchStation searchStation =
                new TabSwitcherSearchStation(/* isIncognito= */ false);
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        searchElement.clickTo().arriveAt(searchStation, softKeyboard);
        softKeyboard.close();
        return searchStation;
    }

    private boolean shouldHubSearchBoxBeVisible() {
        return HubUtils.isScreenWidthTablet(
                mActivityElement.value().getResources().getConfiguration().screenWidthDp);
    }

    // TODO(crbug.com/413652567): Implement actions.
}
