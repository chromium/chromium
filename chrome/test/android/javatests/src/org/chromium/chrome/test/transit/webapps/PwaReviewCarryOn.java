// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.webapps;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.test.transit.CarryOn;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.components.webapps.R;

/** The Review Apps screen inside a bottom sheet. */
public class PwaReviewCarryOn extends CarryOn {
    static final String WEB_APPS_USED_IN_THE_LAST_MONTH = "Web apps used in the last month";
    public final ViewElement<ViewGroup> appListElement;
    public final ViewElement<View> deselectButtonElement;
    public final ViewElement<View> restoreButtonElement;

    public PwaReviewCarryOn() {
        declareActivity(ChromeTabbedActivity.class);
        declareView(withText(WEB_APPS_USED_IN_THE_LAST_MONTH));

        appListElement = declareView(ViewGroup.class, withId(R.id.scroll_view_content));
        deselectButtonElement =
                declareView(withId(R.id.deselect_button), ViewElement.allowDisabledOption());
        restoreButtonElement =
                declareView(withId(R.id.restore_button), ViewElement.allowDisabledOption());
    }

    /** Ensures an entry for an app is shown. */
    public PwaReviewAppEntryCarryOn focusOnEntry(String appName) {
        return noopTo().pickUpCarryOn(new PwaReviewAppEntryCarryOn(this, appName));
    }

    /** Presses back, expecting to return to the initial restore screen. */
    public PwaRestoreCarryOn pressBackToReturn() {
        return pressBackTo().dropCarryOnAnd().pickUpCarryOn(new PwaRestoreCarryOn());
    }
}
