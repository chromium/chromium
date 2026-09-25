// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ui;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewElementMatchesCondition;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;

/**
 * A snackbar shown at the bottom of the Activity.
 *
 * @param <HostStationT> the Station where the Snackbar appears
 */
public class SnackbarFacility<HostStationT extends Station<?>> extends Facility<HostStationT> {
    public static final String NO_BUTTON = "__NO_BUTTON__";

    public @Nullable ViewElement<View> messageElement;
    public @Nullable ViewElement<View> buttonElement;

    public SnackbarFacility(
            @Nullable String expectedMessageSubstring, @Nullable String expectedButtonText) {
        messageElement = declareView(withId(R.id.snackbar_message));
        if (expectedMessageSubstring != null) {
            declareEnterCondition(
                    new ViewElementMatchesCondition(
                            messageElement, withText(containsString(expectedMessageSubstring))));
        }

        Matcher<View> buttonSpec = withId(R.id.snackbar_button);
        if (NO_BUTTON.equals(expectedButtonText)) {
            declareNoView(buttonSpec);
        } else {
            buttonElement = declareView(buttonSpec);
            if (expectedButtonText != null) {
                declareEnterCondition(
                        new ViewElementMatchesCondition(
                                buttonElement, withText(expectedButtonText)));
            }
        }
    }

    /**
     * Programmatically dismisses the snackbar via {@link SnackbarManager#dismissAllSnackbars()}
     * (not a user gesture), exiting this facility and immediately triggering {@link
     * SnackbarManager.SnackbarController#onDismissNoAction(Object)} to commit any pending tab/group
     * closures without waiting for the snackbar timeout.
     */
    public void dismissProgrammatically() {
        runOnUiThreadTo(
                        () -> {
                            assert mHostStation.getActivity() instanceof SnackbarManageable;
                            ((SnackbarManageable) mHostStation.getActivity())
                                    .getSnackbarManager()
                                    .dismissAllSnackbars();
                        })
                .exitFacility();
    }
}
