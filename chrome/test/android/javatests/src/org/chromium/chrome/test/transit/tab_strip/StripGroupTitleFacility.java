// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tab_strip;

import org.chromium.base.test.transit.Facility;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.components.tab_groups.TabGroupId;

/**
 * The title of a tab group on the horizontal tab strip, found by tab group id. Entered with {@link
 * TabStripFacility#expectGroupTitle(TabGroupId)} once the title is clickable on the strip.
 *
 * @param <HostStationT> the type of host {@link CtaPageStation}.
 */
@NullMarked
public class StripGroupTitleFacility<HostStationT extends CtaPageStation>
        extends Facility<HostStationT> {
    private final TabStripFacility<HostStationT> mTabStrip;
    private final TabGroupId mTabGroupId;

    public StripViewElement<StripLayoutGroupTitle> titleElement;

    StripGroupTitleFacility(TabStripFacility<HostStationT> tabStrip, TabGroupId tabGroupId) {
        super("StripGroupTitleFacility");
        mTabStrip = tabStrip;
        mTabGroupId = tabGroupId;
    }

    @Initializer
    @Override
    public void declareExtraElements() {
        titleElement =
                declareElement(
                        new StripViewElement<>(
                                mTabStrip.stripLayoutHelperManagerElement,
                                mHostStation.isIncognito(),
                                "Title of group " + mTabGroupId,
                                this::findGroupTitle,
                                /* optional= */ false));
    }

    /** Returns the {@link TabStripFacility} containing this group title. */
    public TabStripFacility<HostStationT> getTabStrip() {
        return mTabStrip;
    }

    /** Long-presses the group title and opens the tab group context menu. */
    public TabStripGroupContextMenuFacility<HostStationT> openContextMenu() {
        return titleElement.longPressTo().enterFacility(new TabStripGroupContextMenuFacility<>());
    }

    private @Nullable StripLayoutGroupTitle findGroupTitle(StripLayoutHelperManager manager) {
        return manager.getStripLayoutHelper(mHostStation.isIncognito())
                .findGroupTitle(TabGroupId.toOptionalToken(mTabGroupId));
    }
}
