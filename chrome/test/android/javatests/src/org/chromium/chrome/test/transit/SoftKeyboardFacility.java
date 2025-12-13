// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.test.espresso.Espresso;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.ui.test.transit.SoftKeyboardElement;

/** Represents the soft keyboard shown, expecting it to hide after exiting the Facility. */
public class SoftKeyboardFacility extends Facility<Station<?>> {
    public SoftKeyboardElement softKeyboardElement;

    @Override
    public void declareExtraElements() {
        softKeyboardElement =
                declareElement(new SoftKeyboardElement(mHostStation.getActivityElement()));
    }

    /**
     * Close the soft keyboard and wait for the passed ViewElements to stop moving after the
     * relayout.
     *
     * <p>If it was expected to not be shown, just ensure that and exit this Facility.
     *
     * @param viewElementsToSettle the ViewElements to wait to stop moving
     */
    public void close(ViewElement... viewElementsToSettle) {
        assertInPhase(Phase.ACTIVE);

        if (softKeyboardElement.value()) {
            // Keyboard was expected to be shown

            // If this fails, the keyboard was closed before, but not by this facility.
            recheckActiveConditions();

            TripBuilder tripBuilder = runTo(Espresso::closeSoftKeyboard).withRetry();

            if (viewElementsToSettle.length > 0) {
                Facility<?> viewsSettledFacility = new Facility<>("ViewsSettled");
                for (ViewElement<?> viewElement : viewElementsToSettle) {
                    viewsSettledFacility.declareView(
                            viewElement.getViewSpec(),
                            viewElement.copyOptions().initialSettleTime(1000).build());
                }
                tripBuilder = tripBuilder.enterFacilityAnd(viewsSettledFacility);
            }

            tripBuilder.exitFacility();
        } else {
            // Keyboard was not expected to be shown
            noopTo().exitFacility();
        }
    }
}
