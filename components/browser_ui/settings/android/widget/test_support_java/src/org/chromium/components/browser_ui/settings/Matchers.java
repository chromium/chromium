// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.view.View;
import android.widget.TextView;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

/** Contains assert conditions for preference components. */
public class Matchers {
    /**
     * Returns a matcher that checks if a {@link TextView} has a {@link Drawable} set via {@code
     * android:drawableStart}.
     */
    public static Matcher<View> hasDrawableStart() {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View view) {
                return ((TextView) view).getCompoundDrawablesRelative()[0] != null;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("Expected TextView to define android:drawableStart");
            }
        };
    }
}
