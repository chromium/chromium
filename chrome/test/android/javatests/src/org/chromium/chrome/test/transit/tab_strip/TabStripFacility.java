// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tab_strip;

import static org.chromium.base.test.transit.Condition.whether;
import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.tab_search.TabSearchOverlayFacility;

/**
 * The horizontal tab strip of tablets and desktop, entered with {@link
 * CtaPageStation#expectTabStrip()}.
 *
 * <p>Its tabs and buttons are {@link StripViewElement}s: found in the strip model, checked with
 * {@link StripViewCondition} (clickable: a click on the center reaches the view), and clicked with
 * {@link StripLayoutHelperManager#simulateClick}.
 *
 * <p>Strip tabs are {@link StripTabFacility}s, entered with {@link #expectTab(int)}.
 *
 * @param <HostStationT> the type of host {@link CtaPageStation}. The strip isn't shown in the Hub.
 */
@NullMarked
public class TabStripFacility<HostStationT extends CtaPageStation> extends Facility<HostStationT> {
    // Same timeout as TabStripUtils.clickNewTabButton().
    private static final long NEW_TAB_TIMEOUT_MS = 10000L;

    public Element<StripLayoutHelperManager> stripLayoutHelperManagerElement;
    public StripViewElement<CompositorButton> newTabButtonElement;

    /** Declared only with tab search on desktop. */
    public @Nullable StripViewElement<CompositorButton> tabSearchButtonElement;

    /** Declared only when there are incognito tabs in this window. */
    public @Nullable StripViewElement<CompositorButton> modelSelectorButtonElement;

    public TabStripFacility() {
        super("TabStripFacility");
    }

    @Initializer
    @Override
    public void declareExtraElements() {
        boolean incognito = mHostStation.isIncognito();
        stripLayoutHelperManagerElement =
                declareEnterConditionAsElement(
                        new StripLayoutHelperManagerCondition(
                                assumeNonNull(mHostStation.getActivityElement())));
        newTabButtonElement =
                declareElement(
                        new StripViewElement<>(
                                stripLayoutHelperManagerElement,
                                incognito,
                                "New tab button",
                                manager ->
                                        manager.getStripLayoutHelper(incognito).getNewTabButton(),
                                /* optional= */ false));
        // See StripLayoutHelper#createTabSearchButton().
        if (ChromeFeatureList.sTabSearchForDesktop.isEnabled()) {
            tabSearchButtonElement =
                    declareElement(
                            new StripViewElement<>(
                                    stripLayoutHelperManagerElement,
                                    incognito,
                                    "Tab search button",
                                    manager ->
                                            manager.getStripLayoutHelper(incognito)
                                                    .getTabSearchButton(),
                                    /* optional= */ false));
        }
        // See StripLayoutTrailingButtonsCoordinator#shouldModelSelectorButtonBeVisible().
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow() && hasIncognitoTabs()) {
            modelSelectorButtonElement =
                    declareElement(
                            new StripViewElement<>(
                                    stripLayoutHelperManagerElement,
                                    incognito,
                                    "Model selector button",
                                    StripLayoutHelperManager::getModelSelectorButton,
                                    /* optional= */ false));
        }
    }

    /**
     * Expects the tab of |tabId| to be clickable on the strip. Its close button is optional, since
     * it's hidden on narrow, pinned or closing tabs.
     */
    public StripTabFacility<HostStationT> expectTab(int tabId) {
        return noopTo().enterFacility(new StripTabFacility<>(this, tabId));
    }

    /** Clicks the new tab button of the regular strip and arrives at the new tab's NTP. */
    public RegularNewTabPageStation openNewTab() {
        assert !mHostStation.isIncognito() : "Use openNewIncognitoTab() on the incognito strip";
        return newTabButtonElement
                .clickTo()
                .withTimeout(NEW_TAB_TIMEOUT_MS)
                .arriveAt(RegularNewTabPageStation.newBuilder().initOpeningNewTab().build());
    }

    /** Clicks the new tab button of the incognito strip and arrives at the new tab's NTP. */
    public IncognitoNewTabPageStation openNewIncognitoTab() {
        assert mHostStation.isIncognito() : "Use openNewTab() on the regular strip";
        return newTabButtonElement
                .clickTo()
                .withTimeout(NEW_TAB_TIMEOUT_MS)
                .arriveAt(IncognitoNewTabPageStation.newBuilder().initOpeningNewTab().build());
    }

    /**
     * Clicks the model selector button and arrives at the selected tab of the other model (regular
     * or incognito).
     *
     * @param destinationBuilder Builder of the expected {@link CtaPageStation}, e.g. {@code
     *     WebPageStation.newBuilder()}.
     * @return the {@link CtaPageStation} of the other model's selected tab.
     */
    public <PageStationT extends CtaPageStation> PageStationT switchModel(
            BasePageStation.Builder<PageStationT> destinationBuilder) {
        boolean expectIncognito = !mHostStation.isIncognito();
        @Nullable Tab otherModelTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TabModelUtils.getCurrentTab(
                                        mHostStation
                                                .getTabModelSelector()
                                                .getModel(expectIncognito)));
        assert otherModelTab != null : "No selected tab in the other model";
        return switchModelTo()
                .arriveAt(
                        destinationBuilder
                                .withIncognito(expectIncognito)
                                .withTabAlreadySelected(otherModelTab)
                                .build());
    }

    /**
     * Starts a Trip clicking the model selector button, which waits for the other model (regular or
     * incognito) to be selected.
     *
     * <p>Prefer {@link #switchModel}. Completing this Trip without arriving at a Station of the
     * other model leaves the host Station on the wrong model; only legacy tests should do that.
     */
    public TripBuilder switchModelTo() {
        assert modelSelectorButtonElement != null : "No incognito tabs in this window";
        boolean expectIncognito = !mHostStation.isIncognito();
        return modelSelectorButtonElement
                .clickTo()
                .waitForAnd(
                        SimpleConditions.uiThreadCondition(
                                (expectIncognito ? "Incognito" : "Regular") + " model selected",
                                mHostStation.tabModelSelectorElement,
                                selector ->
                                        whether(
                                                selector.isIncognitoSelected() == expectIncognito,
                                                "isIncognitoSelected=%b",
                                                selector.isIncognitoSelected())));
    }

    /** Clicks the tab search button and opens the Tab Search overlay. */
    public TabSearchOverlayFacility<HostStationT> openTabSearchOverlay() {
        assert tabSearchButtonElement != null : "Tab search is off";
        return tabSearchButtonElement
                .clickTo()
                .withTimeout(TabSearchOverlayFacility.OPEN_TIMEOUT_MS)
                .enterFacility(new TabSearchOverlayFacility<>());
    }

    private boolean hasIncognitoTabs() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mHostStation
                                        .getTabModelSelector()
                                        .getModel(/* incognito= */ true)
                                        .getCount()
                                > 0);
    }
}
