// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tab_strip;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Facility;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabClosingCondition;

/**
 * A tab on the horizontal tab strip, found by tab id, like the tab switcher's tab cards. Entered
 * with {@link TabStripFacility#expectTab(int)} once the tab is clickable on the strip that is
 * shown.
 *
 * @param <HostStationT> the type of host {@link CtaPageStation}.
 */
@NullMarked
public class StripTabFacility<HostStationT extends CtaPageStation> extends Facility<HostStationT> {
    private final TabStripFacility<HostStationT> mTabStrip;
    private final int mTabId;

    public StripViewElement<StripLayoutTab> tabElement;

    /** Optional: close buttons are hidden on narrow, pinned or closing tabs. */
    public StripViewElement<CompositorButton> closeButtonElement;

    StripTabFacility(TabStripFacility<HostStationT> tabStrip, int tabId) {
        super("StripTabFacility");
        mTabStrip = tabStrip;
        mTabId = tabId;
    }

    @Initializer
    @Override
    public void declareExtraElements() {
        boolean incognito = mHostStation.isIncognito();
        tabElement =
                declareElement(
                        new StripViewElement<>(
                                mTabStrip.stripLayoutHelperManagerElement,
                                incognito,
                                "Tab " + mTabId,
                                this::findTab,
                                /* optional= */ false));
        closeButtonElement =
                declareElement(
                        new StripViewElement<>(
                                mTabStrip.stripLayoutHelperManagerElement,
                                incognito,
                                "Close button of tab " + mTabId,
                                manager -> {
                                    StripLayoutTab tab = findTab(manager);
                                    return tab == null ? null : tab.getCloseButton();
                                },
                                /* optional= */ true));
    }

    /**
     * Clicks the strip tab to select it.
     *
     * @param destinationBuilder Builder of the expected {@link CtaPageStation}, e.g. {@code
     *     WebPageStation.newBuilder()}.
     * @return the {@link CtaPageStation} of the selected tab.
     */
    public <PageStationT extends CtaPageStation> PageStationT select(
            BasePageStation.Builder<PageStationT> destinationBuilder) {
        assert !isSelected() : "Tab " + mTabId + " is already selected";
        return tabElement
                .clickTo()
                .arriveAt(
                        destinationBuilder
                                .withIncognito(mHostStation.isIncognito())
                                .initSelectingExistingTab()
                                .build());
    }

    /**
     * Clicks the close button of the strip tab and waits for the tab to start closing in the tab
     * model, and for the tab strip to remove it and settle.
     *
     * <p>Only for tabs that are not selected: closing the selected tab selects another one, so the
     * host Station would no longer be accurate.
     */
    public void close() {
        assert !isSelected() : "Closing the selected tab " + mTabId + " is not supported";
        tabElement.expectRemoved();
        closeButtonElement
                .clickTo()
                .waitForAnd(new TabClosingCondition(mHostStation.tabModelElement))
                .exitFacility();
    }

    private boolean isSelected() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = mHostStation.getActivity().getActivityTab();
                    return tab != null && tab.getId() == mTabId;
                });
    }

    private @Nullable StripLayoutTab findTab(StripLayoutHelperManager manager) {
        return manager.getStripLayoutHelper(mHostStation.isIncognito()).findTabById(mTabId);
    }
}
