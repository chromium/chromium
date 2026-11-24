// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tab_strip;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.test.transit.page.CtaPageStation;

/**
 * The context menu of the tab strip's empty space, entered with {@link
 * TabStripFacility#openContextMenu()}.
 *
 * @param <HostStationT> the type of host {@link CtaPageStation}.
 */
@NullMarked
public class TabStripContextMenuFacility<HostStationT extends CtaPageStation>
        extends ScrollableFacility<HostStationT> {
    public final ViewElement<View> menuListElement;

    /** Absent in incognito, disabled when there is only one tab. */
    public Item bookmarkAllTabsItem;

    TabStripContextMenuFacility() {
        menuListElement =
                declareContainerView(
                        View.class,
                        withId(R.id.tab_group_action_menu_list),
                        ViewElement.defaultOptions());
    }

    @Initializer
    @Override
    protected void declareItems(ItemsBuilder items) {
        // See TabStripContextMenuCoordinator#configureMenuItems().
        ViewSpec<View> bookmarkAllTabs = viewSpec(withText(R.string.menu_bookmark_all_tabs));
        if (mHostStation.isIncognito()) {
            bookmarkAllTabsItem = items.declareAbsentItem(bookmarkAllTabs, null);
        } else if (hasMultipleTabs()) {
            bookmarkAllTabsItem = items.declareItem(bookmarkAllTabs, null);
        } else {
            bookmarkAllTabsItem = items.declareDisabledItem(bookmarkAllTabs, null);
        }
    }

    /** Clicks "Bookmark all tabs", which closes the menu. */
    public void bookmarkAllTabs() {
        assert bookmarkAllTabsItem.getPresence() == Item.Presence.PRESENT_AND_ENABLED
                : "\"Bookmark all tabs\" is disabled or absent";
        bookmarkAllTabsItem.scrollToAndSelectTo().complete();
    }

    /** Closes the menu with the back button. */
    public void dismiss() {
        pressBackTo().exitFacility();
    }

    private boolean hasMultipleTabs() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mHostStation.getTabModel().getCount() > 1);
    }
}
