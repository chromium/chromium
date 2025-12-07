// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.view.View;
import android.widget.TextView;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

public class SigninMatchers {
    /**
     * Custom matcher for comparing email strings and displayed email addresses that have been
     * formatted by {@link ExistingAccountRowViewBinder#formatEmailForDisplay(String)}.
     *
     * @param originalEmail The original email string.
     */
    public static Matcher<View> withFormattedEmailText(final String originalEmail) {
        // Unicode character used in ExistingAccountRowViewBinder#formatEmailForDisplay
        // for formatting that needs to be removed from the displayed text before matching.
        String noBreakChar = "\u2060";

        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View item) {
                if (!(item instanceof TextView)) {
                    return false;
                }
                String displayedEmail = ((TextView) item).getText().toString();
                String displayedEmailWithoutSpecialChar = displayedEmail.replace(noBreakChar, "");
                return originalEmail.equals(displayedEmailWithoutSpecialChar);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText(
                        "Matching displayed email with '"
                                + originalEmail
                                + "' ignoring special no-break characters");
            }
        };
    }
}
