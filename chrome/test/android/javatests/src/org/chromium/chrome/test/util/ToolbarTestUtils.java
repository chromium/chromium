// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.core.AllOf.allOf;
import static org.hamcrest.core.IsNot.not;

import androidx.annotation.IdRes;

import org.chromium.chrome.R;

/**
 * A utility class that contains methods generic to both the top toolbar and bottom toolbar, and
 * resource ids of views on the toolbar.
 */
public class ToolbarTestUtils {
    // Res ids of views being tested.
    public static final @IdRes int TOP_TOOLBAR = R.id.toolbar;

    public static final @IdRes int TOP_TOOLBAR_MENU = R.id.menu_button_wrapper;
    public static final @IdRes int TOP_TOOLBAR_HOME = R.id.home_button;
    public static final @IdRes int TOP_TOOLBAR_TAB_SWITCHER = R.id.tab_switcher_button;

    public static void checkToolbarVisibility(@IdRes int toolbarId, boolean isVisible) {
        onView(withId(toolbarId)).check(matches(isVisible ? isDisplayed() : not(isDisplayed())));
    }

    public static void checkToolbarButtonVisibility(
            @IdRes int toolbarId, @IdRes int buttonId, boolean isVisible) {
        // We might have buttons with identical ids on both top toolbar and bottom toolbar,
        // so toolbarId is required in order to get the target view correctly.
        onView(allOf(withId(buttonId), isDescendantOfA(withId(toolbarId))))
                .check(matches(isVisible ? isDisplayed() : not(isDisplayed())));
    }
}
