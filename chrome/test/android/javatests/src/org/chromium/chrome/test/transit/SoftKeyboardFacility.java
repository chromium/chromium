// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.test.espresso.Espresso;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.ui.test.transit.SoftKeyboardElement;

/** Represents the soft keyboard shown, expecting it to hide after exiting the Facility. */
public class SoftKeyboardFacility extends Facility<Station<?>> {
    private SoftKeyboardElement mSoftKeyboardElement;

    @Override
    public void declareElements(Elements.Builder elements) {
        mSoftKeyboardElement =
                elements.declareElement(new SoftKeyboardElement(mHostStation.getActivityElement()));
    }

    public void close() {
        assertSuppliersCanBeUsed();

        if (mSoftKeyboardElement.get()) {
            // Keyboard was expected to be shown

            // If this fails, the keyboard was closed before, but not by this facility.
            recheckActiveConditions();
            mHostStation.exitFacilitySync(this, Espresso::pressBack);
        } else {
            // Keyboard was not expected to be shown
            mHostStation.exitFacilitySync(this, /* trigger= */ null);
        }
    }
}
