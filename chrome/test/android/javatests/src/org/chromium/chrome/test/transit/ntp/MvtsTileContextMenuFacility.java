// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.widget.ListView;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;
import org.chromium.chrome.test.transit.ui.ListMenuFacility;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;

import java.util.List;

/** Facility for a Most Visited Tiles tile's context menu. */
public class MvtsTileContextMenuFacility extends ListMenuFacility<RegularNewTabPageStation> {
    private final MvtsFacility mMvtsFacility;
    private final MvtsTileFacility mMvtsTileFacility;

    public final ViewElement<ListView> menuListElement;
    public Item openInNewTab;
    public Item openInNewTabInGroup;
    public Item openInIncognitoTab;
    public Item openInIncognitoWindow;
    public Item openInOtherWindow;
    public Item downloadLink;
    public Item editShortcut;
    public Item remove;
    public Item pin;
    public Item unpin;

    public MvtsTileContextMenuFacility(
            MvtsFacility mvtsFacility, MvtsTileFacility mvtsTileFacility) {
        super();
        mMvtsFacility = mvtsFacility;
        mMvtsTileFacility = mvtsTileFacility;

        menuListElement =
                declareContainerView(
                        ListView.class, withId(R.id.menu_list), ViewElement.defaultOptions());
    }

    /**
     * Select "Remove" to remove a Top Sites Tile.
     *
     * @param siteSuggestionsAfterRemove The site suggestions after remove.
     * @param fakeMostVisitedSites The fake most visited sites.
     * @return The new {@link MvtRemovedSnackbarFacility} after removal and the Undo Snackbar.
     */
    public MvtRemovedSnackbarFacility selectRemove(
            List<SiteSuggestion> siteSuggestionsAfterRemove,
            FakeMostVisitedSites fakeMostVisitedSites) {
        var mvtsAfterRemove = new MvtsFacility(siteSuggestionsAfterRemove);
        var snackbar = new MvtRemovedSnackbarFacility(mMvtsFacility, mvtsAfterRemove);
        remove.scrollToAndSelectTo()
                .withAdditionalTrigger(
                        () -> {
                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            fakeMostVisitedSites.setTileSuggestions(
                                                    siteSuggestionsAfterRemove));
                        })
                .enterFacilities(mvtsAfterRemove, snackbar);
        return snackbar;
    }

    /**
     * Select "Unpin" to unpin a Custom Link Tile.
     *
     * @param siteSuggestionsAfterUnpin The site suggestions after unpin.
     * @param fakeMostVisitedSites The fake most visited sites.
     * @return The new {@link MvtUnpinnedSnackbarFacility} after unpin and the Undo Snackbar.
     */
    public MvtUnpinnedSnackbarFacility selectUnpin(
            List<SiteSuggestion> siteSuggestionsAfterUnpin,
            FakeMostVisitedSites fakeMostVisitedSites) {
        var mvtsAfterUnpin = new MvtsFacility(siteSuggestionsAfterUnpin);
        var snackbar = new MvtUnpinnedSnackbarFacility(mMvtsFacility, mvtsAfterUnpin);
        unpin.scrollToAndSelectTo()
                .withAdditionalTrigger(
                        () -> {
                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            fakeMostVisitedSites.setTileSuggestions(
                                                    siteSuggestionsAfterUnpin));
                        })
                .enterFacilities(mvtsAfterUnpin, snackbar);
        return snackbar;
    }

    /** Select "Open in new tab" to open the tile in a new tab in background. */
    public void selectOpenInNewTab() {
        openInNewTab
                .scrollToAndSelectTo()
                .waitFor(new TabCountChangedCondition(mHostStation.getTabModel(), +1));
    }

    /** Select "Open in incognito tab" to open the tile in a new incognito tab. */
    public WebPageStation selectOpenInIncognitoTab() {
        String url = mMvtsTileFacility.getSiteSuggestion().url.getSpec();
        return openInIncognitoTab
                .scrollToAndSelectTo()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initOpeningNewTab()
                                .withIncognito(true)
                                .withExpectedUrlSubstring(url)
                                .build());
    }

    /** Select "Open in incognito window" to open the tile in a new incognito window. */
    public WebPageStation selectOpenInIncognitoWindow() {
        String url = mMvtsTileFacility.getSiteSuggestion().url.getSpec();
        return openInIncognitoWindow
                .scrollToAndSelectTo()
                .inNewTask()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .withEntryPoint()
                                .withIncognito(true)
                                .withExpectedUrlSubstring(url)
                                .build());
    }

    @Override
    protected void declareItems(ItemsBuilder items) {
        openInNewTab =
                items.declareItem(
                        withText("Open in new tab"),
                        withMenuItemId(ContextMenuItemId.OPEN_IN_NEW_TAB));
        openInNewTabInGroup =
                items.declareItem(
                        withText("Open in new tab in group"),
                        withMenuItemId(ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP));
        openInIncognitoTab =
                items.declareItem(
                        withText("Open in Incognito tab"),
                        withMenuItemId(ContextMenuItemId.OPEN_IN_INCOGNITO_TAB));
        openInIncognitoWindow =
                items.declareItem(
                        withText("Open in Incognito window"),
                        withMenuItemId(ContextMenuItemId.OPEN_IN_INCOGNITO_WINDOW));
        openInOtherWindow =
                items.declareItem(
                        withText("Open in other window"),
                        withMenuItemId(ContextMenuItemId.OPEN_IN_OTHER_WINDOW));
        downloadLink =
                items.declareItem(
                        withText("Download link"),
                        withMenuItemId(ContextMenuItemId.SAVE_FOR_OFFLINE));
        editShortcut =
                items.declareItem(
                        withText("Edit shortcut"), withMenuItemId(ContextMenuItemId.EDIT_SHORTCUT));
        remove = items.declareItem(withText("Remove"), withMenuItemId(ContextMenuItemId.REMOVE));
        pin =
                items.declareItem(
                        withText("Pin"), withMenuItemId(ContextMenuItemId.PIN_THIS_SHORTCUT));
        unpin = items.declareItem(withText("Unpin"), withMenuItemId(ContextMenuItemId.UNPIN));
    }

    private static Matcher<MVCListAdapter.ListItem> withMenuItemId(@ContextMenuItemId int id) {
        return new TypeSafeMatcher<>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("with menu item id ");
                description.appendText(String.valueOf(id));
            }

            @Override
            protected boolean matchesSafely(MVCListAdapter.ListItem listItem) {
                return listItem.model.get(ListMenuItemProperties.MENU_ITEM_ID) == id;
            }
        };
    }
}
