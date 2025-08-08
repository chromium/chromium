// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.webapps;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.View;

import org.chromium.base.test.transit.CarryOn;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.components.webapps.R;

/** The initial restore screen inside a bottom sheet. */
public class PwaRestoreCarryOn extends CarryOn {
    static final String RESTORE_YOUR_WEB_APPS = "Restore your web apps";
    public final ViewElement<View> reviewButtonElement;

    public PwaRestoreCarryOn() {
        declareActivity(ChromeTabbedActivity.class);
        declareView(withText(RESTORE_YOUR_WEB_APPS));
        reviewButtonElement = declareView(withId(R.id.review_button));
    }

    /** Click the Review button to expand the bottom sheet into the Review Apps screen. */
    public PwaReviewCarryOn clickReview() {
        return reviewButtonElement.clickTo().dropCarryOnAnd().pickUpCarryOn(new PwaReviewCarryOn());
    }
}
