// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.test.espresso.Espresso;

import org.chromium.base.Log;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.ui.test.transit.SoftKeyboardElement;

/** Represents the soft keyboard shown, expecting it to hide after exiting the Facility. */
public class SoftKeyboardFacility extends Facility<Station<?>> {
    private static final String TAG = "Transit";
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

        if (softKeyboardElement.get()) {
            // Keyboard was expected to be shown
            Log.i(TAG, "Recheck soft keyboard is on screen and close it.");

            // If this fails, the keyboard was closed before, but not by this facility.
            recheckActiveConditions();

            TripBuilder tripBuilder = runTo(Espresso::closeSoftKeyboard);
            for (ViewElement<?> viewElement : viewElementsToSettle) {
                tripBuilder = tripBuilder.waitForAnd(viewElement.createSettleCondition());
            }
            tripBuilder.withRetry().exitFacility();
            Log.i(TAG, "Close soft keyboard.");
        } else {
            // Keyboard was not expected to be shown
            Log.i(TAG, "Keyboard was not expected to be shown, do not try to close it.");
            noopTo().exitFacility();
        }
    }
}
