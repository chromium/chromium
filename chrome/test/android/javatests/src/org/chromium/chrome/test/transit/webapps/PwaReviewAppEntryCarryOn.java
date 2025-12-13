// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.webapps;

import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isNotChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.View;
import android.widget.CheckBox;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.CarryOn;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.components.webapps.R;

/** An entry for an app in the Review Apps screen. */
public class PwaReviewAppEntryCarryOn extends CarryOn {
    public final ViewElement<View> appNameElement;
    public final ViewElement<View> entryElement;
    public final ViewElement<CheckBox> checkboxElement;

    public PwaReviewAppEntryCarryOn(PwaReviewCarryOn pwaReview, String appName) {
        Matcher<View> appNameMatcher = withText(appName);
        appNameElement = declareView(appNameMatcher);
        entryElement =
                declareView(
                        appNameElement.ancestor(
                                withParent(
                                        pwaReview.appListElement.getViewSpec().getViewMatcher())));
        checkboxElement =
                declareView(entryElement.descendant(CheckBox.class, withId(R.id.checkbox)));
    }

    public Condition isSelected() {
        return checkboxElement.matches(isChecked());
    }

    public Condition isUnselected() {
        return checkboxElement.matches(isNotChecked());
    }
}
