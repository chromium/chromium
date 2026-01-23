// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.notifications;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ui.BottomSheetFacility;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Bottom Sheet opt in that appears to opt-in to Tips Notifications.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public class TipsOptInBottomSheetFacility<HostStationT extends Station<ChromeTabbedActivity>>
        extends BottomSheetFacility<HostStationT> {
    public final ViewElement<TextView> titleElement;
    public final ViewElement<TextView> descriptionElement;
    public final ViewElement<ButtonCompat> positiveButtonElement;
    public final ViewElement<ButtonCompat> negativeButtonElement;

    /** Constructor. */
    public TipsOptInBottomSheetFacility() {
        declareDescendantView(ImageView.class, withId(R.id.opt_in_logo));
        titleElement = declareDescendantView(TextView.class, withId(R.id.opt_in_title_text));
        descriptionElement =
                declareDescendantView(TextView.class, withId(R.id.opt_in_description_text));
        positiveButtonElement =
                declareDescendantView(ButtonCompat.class, withId(R.id.opt_in_positive_button));
        negativeButtonElement =
                declareDescendantView(ButtonCompat.class, withId(R.id.opt_in_negative_button));
    }

    /** Press the close button to dismiss the opt in bottom sheet. */
    public void clickCloseButton() {
        negativeButtonElement.clickTo().exitFacility();
    }

    /** Press the accept button to accept the opt in bottom sheet. */
    public void clickAcceptButton() {
        positiveButtonElement.clickTo().exitFacility();
    }
}
