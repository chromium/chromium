// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.app.Activity;
import android.view.View;
import android.view.inspector.WindowInspector;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.ui.test.transit.SoftKeyboardElement;

import java.util.List;

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

            TripBuilder tripBuilder = runOnUiThreadTo(this::dismissKeyboard).withRetry();

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

    private void dismissKeyboard() {
        Activity activity = mHostStation.getActivity();
        View currentFocus = activity.getCurrentFocus();
        if (currentFocus != null) {
            KeyboardUtils.hideAndroidSoftKeyboard(currentFocus);
        }
        KeyboardUtils.hideAndroidSoftKeyboard(activity.getWindow().getDecorView());

        List<View> globalWindowViews = WindowInspector.getGlobalWindowViews();
        for (int i = globalWindowViews.size() - 1; i >= 0; i--) {
            View view = globalWindowViews.get(i);
            View focused = view.findFocus();
            if (focused != null) {
                KeyboardUtils.hideAndroidSoftKeyboard(focused);
            }
            KeyboardUtils.hideAndroidSoftKeyboard(view);
        }
    }
}
