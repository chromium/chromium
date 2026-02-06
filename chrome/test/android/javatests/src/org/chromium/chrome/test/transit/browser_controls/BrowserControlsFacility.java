// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.browser_controls;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.TravelException;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.test.transit.page.WebPageStation;

/**
 * Facility representing the Browser Controls (Top Controls).
 *
 * <p>It can be used to assert that the browser controls are in a specific state and/or visibility.
 */
public class BrowserControlsFacility<HostStationT extends WebPageStation>
        extends Facility<HostStationT> {
    public final @BrowserControlsState int expectedConstraints;

    public BrowserControlsFacility(@BrowserControlsState int expectedConstraints) {
        this.expectedConstraints = expectedConstraints;
    }

    /**
     * Waits for the browser controls to be moveable by user gesture.
     *
     * <p>This function requires the browser controls to start fully visible. Then it ensures that
     * at some point the controls can be moved by user gesture. It will then fully cycle the top
     * controls to entirely hidden and back to fully shown.
     *
     * @param activity The ChromeActivity.
     */
    public static void waitForBrowserControlsToBeMoveable(WebPageStation webPageStation) {
        ChromeActivity activity = webPageStation.getActivity();
        BrowserControlsStateProvider browserControlsStateProvider =
                activity.getBrowserControlsManager();

        // Start dragging the page down and try to wait until browser controls hidden.
        int numOfRetries = 10; // Give 10 seconds.
        while (numOfRetries >= 0) {
            Condition controlsMovedCondition =
                    SimpleConditions.instrumentationThreadCondition(
                            "Browser controls moved.",
                            () -> {
                                int offset = browserControlsStateProvider.getTopControlOffset();
                                return Condition.whether(
                                        offset != 0, "TopControls offset = %d", offset);
                            });

            try {
                webPageStation
                        .scrollPageDownWithGestureTo(100)
                        .withTimeout(1000)
                        .waitFor(controlsMovedCondition);
            } catch (TravelException e) {
                if (numOfRetries == 0) throw e;

                numOfRetries -= 1;
                continue;
            }
            break;
        }

        // Restore - drag back to the page top. Start drag has 100 below top controls to avoid
        // starting the drag from the toolbar.
        webPageStation
                .scrollPageUpWithGestureTo()
                .withRetry()
                .waitFor(
                        SimpleConditions.instrumentationThreadCondition(
                                "Browser controls reset to fully shown.",
                                () -> {
                                    int offset = browserControlsStateProvider.getTopControlOffset();
                                    return Condition.whetherEquals(0, offset);
                                }));
    }

    /** Assert top controls are scrollable and physically shown. */
    public static <T extends WebPageStation> BrowserControlsFacility<T> shown(T station) {
        BrowserControlsFacility<T> facility =
                new BrowserControlsFacility<>(BrowserControlsState.BOTH);
        facility.declareEnterCondition(
                BrowserControlsOffsetCondition.showAndScrollable(station.getActivityElement()));
        return facility;
    }

    public static <T extends WebPageStation> BrowserControlsFacility<T> scrolledOff(T station) {
        BrowserControlsFacility<T> facility =
                new BrowserControlsFacility<>(BrowserControlsState.BOTH);
        facility.declareEnterCondition(
                BrowserControlsOffsetCondition.scrolledOff(station.getActivityElement()));
        return facility;
    }

    public static <T extends WebPageStation> BrowserControlsFacility<T> shownAndHeightSynced(
            T station) {
        BrowserControlsFacility<T> facility =
                new BrowserControlsFacility<>(BrowserControlsState.BOTH);
        facility.declareEnterCondition(
                BrowserControlsOffsetCondition.showAndHeightSynced(station.getActivityElement()));
        return facility;
    }

    /** Assert top controls are locked to shown. */
    public static <T extends WebPageStation> BrowserControlsFacility<T> forceShown() {
        return new BrowserControlsFacility<>(BrowserControlsState.SHOWN);
    }

    /** Assert top controls are locked to hidden. */
    public static <T extends WebPageStation> BrowserControlsFacility<T> forceHidden() {
        return new BrowserControlsFacility<>(BrowserControlsState.HIDDEN);
    }

    @Override
    public void declareExtraElements() {
        declareEnterCondition(
                new BrowserControlsStateCondition<>(
                        mHostStation.getActivityElement(),
                        mHostStation.activityTabElement,
                        expectedConstraints));
    }
}
