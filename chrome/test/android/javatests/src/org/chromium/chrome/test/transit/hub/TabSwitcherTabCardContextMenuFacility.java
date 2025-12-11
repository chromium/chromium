// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.widget.list_view.TouchTrackingListView;

/**
 * Facility for a tab switcher card that appears upon long press or right click.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public class TabSwitcherTabCardContextMenuFacility<HostStationT extends TabSwitcherStation>
        extends ScrollableFacility<HostStationT> {
    // TODO(crbug.com/467931387): R.id.tab_group_action_menu_list implies tab group-related
    //  operations. Rename to something more appropriate.
    public final ViewElement<TouchTrackingListView> listElement =
            declareView(TouchTrackingListView.class, withId(R.id.tab_group_action_menu_list));

    @Override
    protected void declareItems(ItemsBuilder items) {}
}
