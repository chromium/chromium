// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.view.View;
import android.widget.ListView;

import androidx.annotation.IdRes;
import androidx.test.espresso.action.GeneralClickAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Tap;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.function.Function;

/**
 * Base class for app menus shown when pressing ("...").
 *
 * @param <HostStationT> the type of host {@link Station} where this app menu is opened.
 */
public abstract class AppMenuFacility<HostStationT extends Station<?>>
        extends ScrollableFacility<HostStationT> {

    public ViewElement<ListView> menuListElement;

    public AppMenuFacility() {
        menuListElement = declareView(ListView.class, withId(R.id.app_menu_list));
    }

    /** Create a new app menu item stub which throws UnsupportedOperationException if selected. */
    protected Item<Void> declareStubMenuItem(ItemsBuilder items, @IdRes int id) {
        return items.declareStubItem(itemViewSpec(withId(id)), itemDataMatcher(id));
    }

    /** Create a new app menu item which runs |selectHandler| when selected. */
    protected <SelectReturnT> Item<SelectReturnT> declareMenuItem(
            ItemsBuilder items,
            @IdRes int id,
            Function<ItemOnScreenFacility<SelectReturnT>, SelectReturnT> selectHandler) {
        return items.declareItem(itemViewSpec(withId(id)), itemDataMatcher(id), selectHandler);
    }

    /** Create a new app menu item which transitions to a |DestinationStationT| when selected. */
    protected <DestinationStationT extends Station<?>>
            Item<DestinationStationT> declareMenuItemToStation(
                    ItemsBuilder items,
                    @IdRes int id,
                    Callable<DestinationStationT> destinationStationFactory) {
        return items.declareItemToStation(
                itemViewSpec(withId(id)), itemDataMatcher(id), destinationStationFactory);
    }

    /** Create a new app menu item which enters a |EnteredFacilityT| when selected. */
    protected <EnteredFacilityT extends Facility> Item<EnteredFacilityT> declareMenuItemToFacility(
            ItemsBuilder items,
            @IdRes int id,
            Callable<EnteredFacilityT> destinationFacilityFactory) {
        return items.declareItemToFacility(
                itemViewSpec(withId(id)), itemDataMatcher(id), destinationFacilityFactory);
    }

    /** Create a new disabled app menu item. */
    protected Item<Void> declareDisabledMenuItem(ItemsBuilder items, @IdRes int id) {
        return items.declareDisabledItem(itemViewSpec(withId(id)), itemDataMatcher(id));
    }

    /** Create a new app menu item expected to be absent. */
    protected Item<Void> declareAbsentMenuItem(ItemsBuilder items, @IdRes int id) {
        return items.declareAbsentItem(itemViewSpec(withId(id)), itemDataMatcher(id));
    }

    /**
     * Placeholder for a stub menu item that may or may not exist.
     *
     * <p>Need to add a placeholder item so that expecting only the first n items includes possible
     * items.
     */
    protected Item<Void> declarePossibleStubMenuItem(ItemsBuilder items, @IdRes int id) {
        return items.declarePossibleStubItem();
    }

    /**
     * Create a new app menu item which may or may not exist, which runs |selectHandler| when
     * selected.
     */
    protected <SelectReturnT> Item<SelectReturnT> declarePossibleMenuItem(
            ItemsBuilder items,
            @IdRes int id,
            Function<ItemOnScreenFacility<SelectReturnT>, SelectReturnT> selectHandler) {
        return items.declarePossibleItem(
                itemViewSpec(withId(id)), itemDataMatcher(id), selectHandler);
    }

    public static final @IdRes int NEW_TAB_ID = R.id.new_tab_menu_id;
    public static final @IdRes int NEW_INCOGNITO_TAB_ID = R.id.new_incognito_tab_menu_id;
    public static final @IdRes int NEW_TAB_GROUP_ID = R.id.new_tab_group_menu_id;
    public static final @IdRes int ADD_TO_GROUP_ID = R.id.add_to_group_menu_id;
    public static final @IdRes int NEW_WINDOW_ID = R.id.new_window_menu_id;
    public static final @IdRes int HISTORY_ID = R.id.open_history_menu_id;
    public static final @IdRes int DELETE_BROWSING_DATA_ID = R.id.quick_delete_menu_id;
    public static final @IdRes int DOWNLOADS_ID = R.id.downloads_menu_id;
    public static final @IdRes int BOOKMARKS_ID = R.id.all_bookmarks_menu_id;
    public static final @IdRes int RECENT_TABS_ID = R.id.recent_tabs_menu_id;
    public static final @IdRes int SHARE_ID = R.id.share_menu_id;
    public static final @IdRes int FIND_IN_PAGE_ID = R.id.find_in_page_id;
    public static final @IdRes int TRANSLATE_ID = R.id.translate_id;
    public static final @IdRes int ADD_TO_HOME_SCREEN_UNIVERSAL_INSTALL_ID = R.id.universal_install;
    public static final @IdRes int OPEN_WEBAPK_ID = R.id.open_webapk_id;
    public static final @IdRes int DESKTOP_SITE_ID = R.id.request_desktop_site_id;
    public static final @IdRes int SETTINGS_ID = R.id.preferences_id;
    public static final @IdRes int HELP_AND_FEEDBACK_ID = R.id.help_id;

    /** Default behavior for "Open new tab". */
    protected RegularNewTabPageStation createNewTabPageStation() {
        return RegularNewTabPageStation.newBuilder()
                .withIsOpeningTabs(1)
                .withIsSelectingTabs(1)
                .build();
    }

    /** Default behavior for "Open new Incognito tab". */
    protected IncognitoNewTabPageStation createIncognitoNewTabPageStation() {
        return IncognitoNewTabPageStation.newBuilder()
                .withIsOpeningTabs(1)
                .withIsSelectingTabs(1)
                .build();
    }

    /** Default behavior for "Open new window". */
    protected RegularNewTabPageStation createNewWindowStation() {
        return RegularNewTabPageStation.newBuilder().withEntryPoint().build();
    }

    /** Default behavior for "Delete browsing data". */
    protected QuickDeleteDialogFacility createQuickDeleteDialogFacility() {
        return new QuickDeleteDialogFacility();
    }

    /** Default behavior for "Settings". */
    protected SettingsStation<MainSettings> createSettingsStation() {
        return new SettingsStation<>(MainSettings.class);
    }

    protected ViewSpec<View> itemViewSpec(Matcher<View> matcher) {
        return menuListElement.descendant(matcher);
    }

    protected static Matcher<ListItem> itemDataMatcher(@IdRes int id) {
        return withMenuItemId(id);
    }

    protected static Matcher<MVCListAdapter.ListItem> withMenuItemId(@IdRes int id) {
        return new TypeSafeMatcher<>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("with menu item id ");
                description.appendText(String.valueOf(id));
            }

            @Override
            protected boolean matchesSafely(MVCListAdapter.ListItem listItem) {
                return listItem.model.get(AppMenuItemProperties.MENU_ITEM_ID) == id;
            }
        };
    }

    /** Clicks outside the menu to close it. */
    public void clickOutsideToClose() {
        GeneralClickAction clickBetweenViewAndLeftEdge =
                new GeneralClickAction(
                        Tap.SINGLE,
                        view -> {
                            int[] menuListXy = new int[2];
                            view.getLocationOnScreen(menuListXy);
                            float clickX = (float) menuListXy[0] / 2;
                            assert clickX > 0 : "No space between app menu and edge of screen";
                            float clickY = menuListXy[1];
                            return new float[] {clickX, clickY};
                        },
                        Press.FINGER);
        mHostStation.exitFacilitySync(
                this, menuListElement.getPerformTrigger(clickBetweenViewAndLeftEdge));
    }

    /** Close the menu programmatically. */
    public void closeProgrammatically() {
        mHostStation.exitFacilitySync(
                this,
                Transition.runTriggerOnUiThreadOption(),
                () -> getAppMenuCoordinator().getAppMenuHandler().hideAppMenu());
    }

    /** Verify that the menu model has the expected menu item ids and nothing beyond them. */
    public void verifyModelItems(List<Integer> expectedPresentItemIds) {
        AppMenuCoordinator appMenuCoordinator = getAppMenuCoordinator();
        for (Integer itemId : expectedPresentItemIds) {
            assertNotNull(
                    "Expected item with id "
                            + mHostStation.getActivity().getResources().getResourceName(itemId),
                    AppMenuTestSupport.getMenuItemPropertyModel(appMenuCoordinator, itemId));
        }

        MVCListAdapter.ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(appMenuCoordinator);
        assertEquals(
                "Menu model has more items than expected",
                expectedPresentItemIds.size(),
                menuItemsModelList.size());
    }

    public abstract AppMenuCoordinator getAppMenuCoordinator();
}
