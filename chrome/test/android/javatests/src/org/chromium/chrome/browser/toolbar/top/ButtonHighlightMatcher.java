// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.graphics.drawable.LayerDrawable;
import android.view.View;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.components.browser_ui.widget.highlight.PulseDrawable;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighterTestUtils;

/** A custom matcher that checks buttons for highlighting. */
public class ButtonHighlightMatcher extends TypeSafeMatcher<View> {
    private final Boolean mExpectedToBeHighlighted;

    /**
     * @param expectedToBeHighlighted Whether highlighted is expected.
     * @return A custom matcher to check highlighting.
     */
    public static Matcher<View> withHighlight(boolean expectedToBeHighlighted) {
        return new ButtonHighlightMatcher(expectedToBeHighlighted);
    }

    private ButtonHighlightMatcher(boolean expectedToBeHighlighted) {
        mExpectedToBeHighlighted = expectedToBeHighlighted;
    }

    @Override
    public void describeTo(Description description) {
        description.appendText("Expecting highlighted: " + mExpectedToBeHighlighted);
    }

    @Override
    protected boolean matchesSafely(View view) {
        boolean actuallyHighlighted = false;
        // Approach directly implemented by some toolbar buttons.
        if (view.getBackground() instanceof PulseDrawable) {
            actuallyHighlighted = true;
        } else if (view.getBackground() instanceof LayerDrawable) {
            // Handles ViewHighlighter's approach.
            actuallyHighlighted = ViewHighlighterTestUtils.checkHighlightOn(view);
        }
        return mExpectedToBeHighlighted == actuallyHighlighted;
    }
}
