// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.vertical_tabs;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.view.View;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.tab_search.TabSearchOverlayFacility;

/**
 * The vertical tabs rail shown instead of the horizontal tab strip.
 *
 * <p>Enter it with {@link CtaPageStation#expectVerticalTabs()}.
 *
 * @param <HostStationT> the type of host {@link CtaPageStation} where the rail is displayed.
 */
@NullMarked
public class VerticalTabsFacility<HostStationT extends CtaPageStation>
        extends Facility<HostStationT> {
    public final ViewElement<View> railElement;
    // Only shown when Tab Search for desktop is enabled.
    public final @Nullable ViewElement<View> tabSearchButtonElement;

    public VerticalTabsFacility() {
        super("VerticalTabsFacility");
        railElement =
                declareView(withId(R.id.vertical_tab_rail_container), ViewElement.unscopedOption());
        tabSearchButtonElement =
                ChromeFeatureList.sTabSearchForDesktop.isEnabled()
                        ? declareView(
                                railElement.descendant(withId(R.id.tab_search_button)),
                                ViewElement.unscopedOption())
                        : null;
    }

    /** Opens the Tab Search overlay by clicking the tab search button in the rail. */
    public TabSearchOverlayFacility<HostStationT> openTabSearchOverlay() {
        assert tabSearchButtonElement != null : "Tab Search for desktop is disabled";
        return tabSearchButtonElement
                .clickTo()
                .withTimeout(TabSearchOverlayFacility.OPEN_TIMEOUT_MS)
                .enterFacility(new TabSearchOverlayFacility<>());
    }
}
