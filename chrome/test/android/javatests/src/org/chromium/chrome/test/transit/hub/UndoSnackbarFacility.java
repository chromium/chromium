// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.chrome.test.transit.ui.SnackbarFacility;

/**
 * The snackbar that lets the user undo a group or close operation for a short time.
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
public class UndoSnackbarFacility<HostStationT extends Station<?>>
        extends SnackbarFacility<HostStationT> {

    public UndoSnackbarFacility(String message) {
        super(message, "Undo");
    }

    /** Press undo to revert the operation. */
    public void pressUndo() {
        pressUndoTo().complete();
    }

    /** Start a transition by pressing undo to revert the operation. */
    public TripBuilder pressUndoTo() {
        return buttonElement.clickTo().exitFacilityAnd();
    }
}
