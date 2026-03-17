// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.containsString;

import static org.chromium.base.test.transit.ViewElement.unscopedOption;

import android.view.View;

import com.google.android.material.tabs.TabLayout;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.hub.HubToolbarView;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeActivityTabModelBoundStation;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.layouts.LayoutTypeVisibleCondition;
import org.chromium.ui.base.DeviceFormFactor;

/** The base station for Hub, with several panes and a toolbar. */
public abstract class HubBaseStation
        extends ChromeActivityTabModelBoundStation<ChromeTabbedActivity> {
    public final ViewElement<HubToolbarView> toolbarElement;
    public final ViewElement<View> paneHostElement;
    public ViewElement<View> viewHolderElement;
    public final ViewElement<View> menuButtonElement;
    public final ViewElement<TabLayout> paneSwitcherElement;
    public final @Nullable ViewElement<View> regularTabsButtonElement;
    public final @Nullable ViewElement<View> incognitoTabsButtonElement;
    protected final boolean mIsStandaloneIncognitoWindow;
    protected final boolean mIncognitoTabsExist;
    protected final boolean mRegularTabsExist;

    public HubBaseStation(
            boolean isIncognito,
            boolean regularTabsExist,
            boolean incognitoTabsExist,
            boolean hasMenuButton) {
        super(ChromeTabbedActivity.class, isIncognito);
        mRegularTabsExist = regularTabsExist;
        mIncognitoTabsExist = incognitoTabsExist;

        toolbarElement = declareView(HubToolbarView.class, withId(R.id.hub_toolbar));
        paneHostElement = declareView(withId(R.id.hub_pane_host));
        viewHolderElement = declareView(withId(R.id.tab_switcher_view_holder));
        menuButtonElement =
                hasMenuButton
                        ? declareView(toolbarElement.descendant(withId(R.id.menu_button)))
                        : null;

        mIsStandaloneIncognitoWindow = IncognitoUtils.shouldOpenIncognitoAsWindow() && isIncognito;
        paneSwitcherElement =
                mIsStandaloneIncognitoWindow
                        ? null
                        : declareView(
                                toolbarElement.descendant(
                                        TabLayout.class, withId(R.id.pane_switcher)));

        // TODO(crbug.com/386819654): Add a member of type ViewElement representing tab group pane
        // The non-regular toggle tab button contentDescription is a substring found in the string:
        // R.string.accessibility_tab_switcher_standard_stack.
        regularTabsButtonElement =
                mIsStandaloneIncognitoWindow
                        ? null
                        : declareView(withContentDescription(containsString("standard tab")));
        if (mIncognitoTabsExist && !mIsStandaloneIncognitoWindow) {
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
     * Selects the tab switcher pane on the Hub.
     *
     * @return the corresponding subclass of {@link HubBaseStation}.
     */
    private <T extends HubBaseStation> T selectPane(
            @PaneId int paneId, Class<T> expectedDestination) {
        if (getPaneId() == paneId) {
            return expectedDestination.cast(this);
        }

        return selectPaneTo(paneId, expectedDestination).complete().get(expectedDestination);
    }

    private <T extends HubBaseStation> TripBuilder selectPaneTo(
            int paneId, Class<T> expectedDestination) {
        recheckActiveConditions();

        T destinationStation =
                expectedDestination.cast(
                        HubStationUtils.createHubStation(
                                paneId, mRegularTabsExist, mIncognitoTabsExist));

        String contentDescription =
                HubStationUtils.getContentDescriptionSubstringForIdPaneSelection(paneId);
        SwitchPaneButtonFacility button =
                noopTo().enterFacility(new SwitchPaneButtonFacility(contentDescription));
        return button.buttonElement.clickTo().arriveAtAnd(destinationStation);
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
        if (getPaneId() == PaneId.HISTORY) {
            return HistoryPaneStation.class.cast(this);
        }

        TripBuilder tripBuilder = selectPaneTo(PaneId.HISTORY, HistoryPaneStation.class);

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getActivity())) {
            Trip trip = tripBuilder.enterFacilityAnd(new SoftKeyboardFacility()).complete();
            trip.get(SoftKeyboardFacility.class).close();
            return trip.get(HistoryPaneStation.class);
        } else {
            return tripBuilder.complete().get(HistoryPaneStation.class);
        }
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
