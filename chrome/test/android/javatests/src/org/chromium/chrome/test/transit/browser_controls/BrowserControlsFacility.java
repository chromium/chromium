// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.browser_controls;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.TravelException;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.util.function.Function;
import java.util.function.Supplier;

/**
 * The top browser controls in one state: their constraints and their physical offset.
 *
 * <p>Each state is a separate Facility; exit one and enter another when the controls move.
 */
@NullMarked
public class BrowserControlsFacility<HostStationT extends WebPageStation>
        extends Facility<HostStationT> {
    private final @Nullable @BrowserControlsState Integer mExpectedConstraints;
    private final Function<Supplier<@Nullable ChromeTabbedActivity>, Condition>
            mOffsetConditionFactory;

    private BrowserControlsFacility(
            String name,
            @Nullable @BrowserControlsState Integer expectedConstraints,
            Function<Supplier<@Nullable ChromeTabbedActivity>, Condition> offsetConditionFactory) {
        super(name);
        mExpectedConstraints = expectedConstraints;
        mOffsetConditionFactory = offsetConditionFactory;
    }

    /** The controls can be scrolled and are fully shown. */
    public static <T extends WebPageStation> BrowserControlsFacility<T> shown() {
        return new BrowserControlsFacility<>(
                "BrowserControlsShown",
                BrowserControlsState.BOTH,
                BrowserControlsOffsetCondition::shown);
    }

    /** The controls can be scrolled and are scrolled off. */
    public static <T extends WebPageStation> BrowserControlsFacility<T> scrolledOff() {
        return new BrowserControlsFacility<>(
                "BrowserControlsScrolledOff",
                BrowserControlsState.BOTH,
                BrowserControlsOffsetCondition::scrolledOff);
    }

    /**
     * The controls are fully shown and can't scroll off (min height equals height).
     *
     * <p>Constraints aren't checked: they are SHOWN in a desktop window, where AppHeaderCoordinator
     * locks the controls, and BOTH otherwise.
     */
    public static <T extends WebPageStation> BrowserControlsFacility<T> shownAndHeightSynced() {
        return new BrowserControlsFacility<>(
                "BrowserControlsShownAndHeightSynced",
                /* expectedConstraints= */ null,
                BrowserControlsOffsetCondition::shownAndHeightSynced);
    }

    @Override
    public void declareExtraElements() {
        Supplier<@Nullable ChromeTabbedActivity> activitySupplier =
                assumeNonNull(mHostStation.getActivityElement());
        if (mExpectedConstraints != null) {
            declareEnterCondition(
                    new BrowserControlsStateCondition<>(
                            activitySupplier,
                            mHostStation.activityTabElement,
                            mExpectedConstraints));
        }
        declareEnterCondition(mOffsetConditionFactory.apply(activitySupplier));
    }

    /**
     * Waits for the browser controls to be moveable by user gesture.
     *
     * <p>This function requires the browser controls to start fully visible. Then it ensures that
     * at some point the controls can be moved by user gesture. It will then fully cycle the top
     * controls to entirely hidden and back to fully shown.
     *
     * @param webPageStation The active {@link WebPageStation} to scroll.
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
}
