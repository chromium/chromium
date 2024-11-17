// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.edge_to_edge;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.test.transit.edge_to_edge.EdgeToEdgeConditions.BottomChinCondition;
import org.chromium.chrome.test.transit.edge_to_edge.EdgeToEdgeConditions.BottomControlsStackerCondition;
import org.chromium.chrome.test.transit.edge_to_edge.EdgeToEdgeConditions.EdgeToEdgeControllerCondition;

/**
 * Facility used to capture the bottom chin. The chin is a browser control layer, implemented by
 * EdgeToEdgeBottomChinMediator. Visually, the chin doesn't has to be visible.
 *
 * @param <CtaStationT> A station that lives in ChromeTabbedActivity.
 */
public class EdgeToEdgeBottomChinFacility<CtaStationT extends Station<ChromeTabbedActivity>>
        extends Facility<CtaStationT> {
    private final Boolean mExpectPageOptIn;
    private Supplier<BottomControlsLayer> mEdgeToEdgeBottomChin;
    private Supplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;

    /**
     * Create the facility that wait until bottom chin exists.
     *
     * @param expectedPageOptIn Optional expected result for
     *     EdgeToEdgeController#isPageOptedIntoEdgeToEdge()
     */
    public EdgeToEdgeBottomChinFacility(Boolean expectedPageOptIn) {
        mExpectPageOptIn = expectedPageOptIn;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        mEdgeToEdgeControllerSupplier =
                elements.declareEnterCondition(
                        new EdgeToEdgeControllerCondition(mHostStation.getActivityElement()));
        Supplier<BottomControlsStacker> bottomControlsStackerSupplier =
                elements.declareEnterCondition(
                        new BottomControlsStackerCondition(mHostStation.getActivityElement()));

        mEdgeToEdgeBottomChin =
                elements.declareEnterCondition(
                        new BottomChinCondition(bottomControlsStackerSupplier));

        if (mExpectPageOptIn != null) {
            elements.declareEnterCondition(
                    UiThreadCondition.from(
                            "Expected page opt-in edge to edge: " + mExpectPageOptIn,
                            this::isPageOptInEdgeToEdge));
        }
    }

    /** Return the bottom chin in the page. */
    public BottomControlsLayer getBottomChin() {
        return mEdgeToEdgeBottomChin.get();
    }

    /**
     * @see EdgeToEdgeController#isPageOptedIntoEdgeToEdge()
     */
    public ConditionStatus isPageOptInEdgeToEdge() {
        if (mEdgeToEdgeControllerSupplier.hasValue()
                && mEdgeToEdgeControllerSupplier.get().isPageOptedIntoEdgeToEdge()
                        == mExpectPageOptIn) {
            return Condition.fulfilled();
        }
        return Condition.awaiting("Pending edge to edge page opt-in status.");
    }
}
