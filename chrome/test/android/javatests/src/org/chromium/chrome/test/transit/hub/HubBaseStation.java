// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.containsString;

import static org.chromium.base.test.transit.ViewElement.unscopedOption;
import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.espresso.Espresso;

import com.google.android.material.tabs.TabLayout;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.hub.HubToolbarView;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.R;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.transit.layouts.LayoutTypeVisibleCondition;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.tabmodel.TabModelSelectorCondition;

/** The base station for Hub, with several panes and a toolbar. */
public abstract class HubBaseStation extends Station<ChromeTabbedActivity> {
    public final Element<TabModelSelector> tabModelSelectorElement;
    public final ViewElement<HubToolbarView> toolbarElement;
    public final ViewElement<View> paneHostElement;
    public final ViewElement<View> menuButtonElement;
    public final ViewElement<TabLayout> paneSwitcherElement;
    public final @Nullable ViewElement<View> regularTabsButtonElement;
    public final @Nullable ViewElement<View> incognitoTabsButtonElement;
    protected final boolean mIncognitoTabsExist;
    protected final boolean mRegularTabsExist;

    public HubBaseStation(
            boolean regularTabsExist, boolean incognitoTabsExist, boolean hasMenuButton) {
        super(ChromeTabbedActivity.class);
        mRegularTabsExist = regularTabsExist;
        mIncognitoTabsExist = incognitoTabsExist;

        tabModelSelectorElement =
                declareEnterConditionAsElement(new TabModelSelectorCondition(mActivityElement));

        toolbarElement = declareView(viewSpec(HubToolbarView.class, withId(R.id.hub_toolbar)));
        paneHostElement = declareView(withId(R.id.hub_pane_host));
        menuButtonElement =
                hasMenuButton
                        ? declareView(toolbarElement.descendant(withId(R.id.menu_button)))
                        : null;

        paneSwitcherElement =
                declareView(toolbarElement.descendant(TabLayout.class, withId(R.id.pane_switcher)));

        // TODO(crbug.com/386819654): Add a member of type ViewElement representing tab group pane
        // The non-regular toggle tab button contentDescription is a substring found in the string:
        // R.string.accessibility_tab_switcher_standard_stack.
        regularTabsButtonElement =
                declareView(withContentDescription(containsString("standard tab")));
        if (mIncognitoTabsExist) {
            incognitoTabsButtonElement =
                    declareView(
                            withContentDescription(
                                    R.string.accessibility_tab_switcher_incognito_stack));
        } else {
            incognitoTabsButtonElement = null;
        }

        declareEnterCondition(
                new LayoutTypeVisibleCondition(mActivityElement, LayoutType.TAB_SWITCHER));
    }

    /** Returns the station's {@link PaneId}. */
    public abstract @PaneId int getPaneId();

    /**
     * Returns to the previous tab via the back button.
     *
     * @return the {@link PageStation} that Hub returned to.
     */
    public <T extends PageStation> T leaveHubToPreviousTabViaBack(T destination) {
        return travelToSync(destination, Transition.retryOption(), () -> Espresso.pressBack());
    }

    /**
     * Selects the tab switcher pane on the Hub.
     *
     * @return the corresponding subclass of {@link HubBaseStation}.
     */
    public <T extends HubBaseStation> T selectPane(
            @PaneId int paneId, Class<T> expectedDestination) {
        recheckActiveConditions();

        if (getPaneId() == paneId) {
            return expectedDestination.cast(this);
        }

        T destinationStation =
                expectedDestination.cast(
                        HubStationUtils.createHubStation(
                                paneId, mRegularTabsExist, mIncognitoTabsExist));

        String contentDescription =
                HubStationUtils.getContentDescriptionSubstringForIdPaneSelection(paneId);
        SwitchPaneButtonFacility button =
                enterFacilitySync(
                        new SwitchPaneButtonFacility(contentDescription), /* trigger= */ null);
        return travelToSync(destinationStation, button.buttonElement.getClickTrigger());
    }

    /** Convenience method to select the Regular Tab Switcher pane. */
    public RegularTabSwitcherStation selectRegularTabsPane() {
        return selectPane(PaneId.TAB_SWITCHER, RegularTabSwitcherStation.class);
    }

    /** Convenience method to select the Incognito Tab Switcher pane. */
    public IncognitoTabSwitcherStation selectIncognitoTabsPane() {
        return selectPane(PaneId.INCOGNITO_TAB_SWITCHER, IncognitoTabSwitcherStation.class);
    }

    /** Convenience method to select the Tab Groups pane. */
    public TabGroupPaneStation selectTabGroupsPane() {
        return selectPane(PaneId.TAB_GROUPS, TabGroupPaneStation.class);
    }

    /** Convenience method to select the History pane. */
    public HistoryPaneStation selectHistoryPane() {
        return selectPane(PaneId.HISTORY, HistoryPaneStation.class);
    }

    public class SwitchPaneButtonFacility extends Facility<HubBaseStation> {
        public final ViewElement<View> buttonElement;

        public SwitchPaneButtonFacility(String contentDescription) {
            // TODO(crbug.com/40287437): Content description seems reasonable for now, this might
            // get harder once we use a recycler view with text based buttons.
            buttonElement =
                    declareView(
                            paneSwitcherElement.descendant(
                                    withContentDescription(containsString(contentDescription))),
                            unscopedOption());
        }
    }
}
