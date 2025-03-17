// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.test.espresso.Espresso;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.ui.test.transit.SoftKeyboardElement;

/** Represents the soft keyboard shown, expecting it to hide after exiting the Facility. */
public class SoftKeyboardFacility extends Facility<Station<?>> {
    private SoftKeyboardElement mSoftKeyboardElement;

    @Override
    public void declareElements(Elements.Builder elements) {
        mSoftKeyboardElement =
                elements.declareElement(new SoftKeyboardElement(mHostStation.getActivityElement()));
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
        assertSuppliersCanBeUsed();

        if (mSoftKeyboardElement.get()) {
            // Keyboard was expected to be shown

            // If this fails, the keyboard was closed before, but not by this facility.
            recheckActiveConditions();
            Transition.TransitionOptions.Builder options = Transition.newOptions();
            for (ViewElement viewElement : viewElementsToSettle) {
                options.withCondition(viewElement.createSettleCondition());
            }
            mHostStation.exitFacilitySync(this, options.build(), Espresso::pressBack);
        } else {
            // Keyboard was not expected to be shown
            mHostStation.exitFacilitySync(this, /* trigger= */ null);
        }
    }
}
