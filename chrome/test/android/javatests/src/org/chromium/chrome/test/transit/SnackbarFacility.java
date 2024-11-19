// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import androidx.annotation.Nullable;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.test.R;

/**
 * A snackbar shown at the bottom of the Activity.
 *
 * @param <HostStationT> the Station where the Snackbar appears
 */
public class SnackbarFacility<HostStationT extends Station<?>> extends Facility<HostStationT> {
    public static final ViewSpec SNACKBAR_MESSAGE = viewSpec(withId(R.id.snackbar_message));
    public static final ViewSpec SNACKBAR_BUTTON = viewSpec(withId(R.id.snackbar_button));

    private final String mExpectedMessageSubstring;
    private final String mExpectedButtonText;

    public SnackbarFacility(
            @Nullable String expectedMessageSubstring, @Nullable String expectedButtonText) {
        super();
        mExpectedMessageSubstring = expectedMessageSubstring;
        mExpectedButtonText = expectedButtonText;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        if (mExpectedMessageSubstring != null) {
            ViewSpec messageSpec =
                    viewSpec(
                            withText(containsString(mExpectedMessageSubstring)),
                            SNACKBAR_MESSAGE.getViewMatcher());
            elements.declareView(messageSpec);
        } else {
            elements.declareView(SNACKBAR_MESSAGE);
        }

        if (mExpectedButtonText != null) {
            ViewSpec messageSpec =
                    viewSpec(withText(mExpectedButtonText), SNACKBAR_BUTTON.getViewMatcher());
            elements.declareView(messageSpec);
        } else {
            elements.declareView(SNACKBAR_BUTTON);
        }
    }
}
