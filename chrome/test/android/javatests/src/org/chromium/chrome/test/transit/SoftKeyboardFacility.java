// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.app.Activity;

import androidx.test.espresso.Espresso;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.ui.test.transit.SoftKeyboardElement;

/** Represents the soft keyboard shown, expecting it to hide after exiting the Facility. */
public class SoftKeyboardFacility extends Facility<Station> {
    private final Supplier<? extends Activity> mActivitySupplier;

    public SoftKeyboardFacility(Supplier<? extends Activity> activitySupplier) {
        mActivitySupplier = activitySupplier;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareElement(new SoftKeyboardElement(mActivitySupplier));
    }

    public void close() {
        mHostStation.exitFacilitySync(this, Espresso::pressBack);
    }
}
