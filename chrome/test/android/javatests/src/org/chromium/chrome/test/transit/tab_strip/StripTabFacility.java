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
import org.chromium.chrome.browser.incognito.IncognitoUtils;
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

    public StripLayoutTab getStripLayoutTab() {
        return tabElement.value();
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
     * host Station would no longer be accurate. Use {@link #closeSelected} for the selected tab.
     */
    public void close() {
        assert !isSelected() : "Tab " + mTabId + " is selected, use closeSelected()";
        tabElement.expectRemoved();
        closeButtonElement
                .clickTo()
                .waitForAnd(new TabClosingCondition(mHostStation.tabModelElement))
                .exitFacility();
    }

    /**
     * Clicks the close button of the selected strip tab, waits for the tab strip to remove it and
     * settle, and arrives at the tab selected next in the same TabModel.
     *
     * @param destinationBuilder Builder of the expected {@link CtaPageStation}, e.g. {@code
     *     WebPageStation.newBuilder()}.
     * @return the {@link CtaPageStation} of the tab selected next.
     */
    public <PageStationT extends CtaPageStation> PageStationT closeSelected(
            BasePageStation.Builder<PageStationT> destinationBuilder) {
        assert isSelected() : "Tab " + mTabId + " is not selected, use close()";
        assert getTabCount() > 1
                : "Tab "
                        + mTabId
                        + " is the last tab; closeSelected() requires another tab in the model";
        tabElement.expectRemoved();
        return closeButtonElement.clickTo().reach(mHostStation.nextTabSelected(destinationBuilder));
    }

    /**
     * Clicks the close button of the last incognito tab when at least one regular tab exists, and
     * arrives at the regular model's selected tab. The incognito strip goes away, so it doesn't
     * wait for it to settle.
     *
     * @param destinationBuilder Builder of the expected regular {@link CtaPageStation}, e.g. {@code
     *     WebPageStation.newBuilder()}.
     * @return the {@link CtaPageStation} of the regular model's selected tab.
     */
    public <PageStationT extends CtaPageStation> PageStationT closeLastIncognitoTab(
            BasePageStation.Builder<PageStationT> destinationBuilder) {
        assert mHostStation.isIncognito() : "Not an incognito tab";
        assert getTabCount() == 1 : "Tab " + mTabId + " is not the last incognito tab";
        assert !IncognitoUtils.shouldOpenIncognitoAsWindow()
                : "Closing the last tab of an incognito window closes it, use"
                        + " closeLastTabOfIncognitoWindow()";
        return closeButtonElement
                .clickTo()
                .waitForAnd(new TabClosingCondition(mHostStation.tabModelElement))
                .reach(mTabStrip.otherModelSelected(destinationBuilder));
    }

    /**
     * Clicks the close button of the only tab of an incognito window, which closes the window, and
     * waits for its Activity to be destroyed.
     */
    public void closeLastTabOfIncognitoWindow() {
        assert IncognitoUtils.shouldOpenIncognitoAsWindow() && mHostStation.isIncognito()
                : "Not an incognito window";
        assert getTabCount() == 1 : "Tab " + mTabId + " is not the only tab of the window";
        closeButtonElement.clickTo().reachLastStop();
    }

    private int getTabCount() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mHostStation.getTabModel().getCount());
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
