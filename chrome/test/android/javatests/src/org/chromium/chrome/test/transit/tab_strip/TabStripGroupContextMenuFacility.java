// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tab_strip;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;
import android.widget.EditText;

import org.chromium.base.test.transit.OptionalViewElement;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.test.transit.page.CtaPageStation;

/**
 * The context menu of a tab group title on the tab strip, entered with {@link
 * StripGroupTitleFacility#openContextMenu()}. Shared tab groups are not supported.
 *
 * @param <HostStationT> the type of host {@link CtaPageStation}.
 */
@NullMarked
public class TabStripGroupContextMenuFacility<HostStationT extends CtaPageStation>
        extends ScrollableFacility<HostStationT> {
    public final ViewElement<View> menuListElement;

    /** Optional: can be scrolled off-screen in short windows, e.g. in multi-window. */
    public final OptionalViewElement<EditText> titleElement;

    public Item newTabInGroupItem;
    public Item ungroupItem;
    public Item closeGroupItem;

    /** Absent in incognito. */
    public Item deleteGroupItem;

    TabStripGroupContextMenuFacility() {
        menuListElement =
                declareContainerView(
                        View.class,
                        withId(R.id.tab_group_action_menu_list),
                        ViewElement.defaultOptions());
        titleElement = declareOptionalView(EditText.class, withId(R.id.tab_group_title));
    }

    @Initializer
    @Override
    protected void declareItems(ItemsBuilder items) {
        // See TabGroupContextMenuCoordinator#buildMenuActionItems().
        newTabInGroupItem =
                items.declareItem(
                        viewSpec(withText(R.string.open_new_tab_in_group_context_menu_item)),
                        /* offScreenDataMatcher= */ null);
        ungroupItem =
                items.declareItem(
                        viewSpec(withText(R.string.ungroup_tab_group_menu_item)),
                        /* offScreenDataMatcher= */ null);
        closeGroupItem =
                items.declareItem(
                        viewSpec(withText(R.string.tab_grid_dialog_toolbar_close_group)),
                        /* offScreenDataMatcher= */ null);
        ViewSpec<View> deleteGroup =
                viewSpec(withText(R.string.tab_grid_dialog_toolbar_delete_group));
        if (mHostStation.isIncognito()) {
            deleteGroupItem =
                    items.declareAbsentItem(deleteGroup, /* offScreenDataMatcher= */ null);
        } else {
            deleteGroupItem = items.declareItem(deleteGroup, /* offScreenDataMatcher= */ null);
        }
    }

    /** Starts a Trip clicking "New tab in group", which closes the menu. */
    public TripBuilder openNewTabInGroupTo() {
        return newTabInGroupItem.scrollToAndSelectTo();
    }

    /** Starts a Trip clicking "Ungroup", which closes the menu. */
    public TripBuilder ungroupTo() {
        return ungroupItem.scrollToAndSelectTo();
    }

    /** Starts a Trip clicking "Close group", which closes the menu. */
    public TripBuilder closeGroupTo() {
        return closeGroupItem.scrollToAndSelectTo();
    }

    /** Starts a Trip clicking "Delete group", which closes the menu. */
    public TripBuilder deleteGroupTo() {
        return deleteGroupItem.scrollToAndSelectTo();
    }
}
