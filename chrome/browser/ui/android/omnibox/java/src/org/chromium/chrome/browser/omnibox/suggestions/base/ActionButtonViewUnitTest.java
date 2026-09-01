// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link ActionButtonView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionButtonViewUnitTest {
    private ActionButtonView mView;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        mView = new ActionButtonView(context);
    }

    @Test
    public void notShowOnlyOnFocusButton() {
        mView.enableShowOnlyOnFocus(false);
        assertEquals(View.VISIBLE, mView.getVisibility());

        mView.onParentViewSelected(true);
        assertEquals(View.VISIBLE, mView.getVisibility());

        mView.onParentViewSelected(false);
        assertEquals(View.VISIBLE, mView.getVisibility());

        mView.onParentViewHoverChanged(true);
        assertEquals(View.VISIBLE, mView.getVisibility());

        mView.onParentViewHoverChanged(false);
        assertEquals(View.VISIBLE, mView.getVisibility());
    }

    @Test
    public void showOnlyOnFocusButton_selected() {
        mView.enableShowOnlyOnFocus(true);
        assertEquals(View.INVISIBLE, mView.getVisibility());

        mView.onParentViewSelected(true);
        assertEquals(View.VISIBLE, mView.getVisibility());

        mView.onParentViewSelected(false);
        assertEquals(View.INVISIBLE, mView.getVisibility());
    }

    @Test
    public void showOnlyOnFocusButton_selectedBeforeEnabled() {
        mView.onParentViewSelected(true);
        assertEquals(View.VISIBLE, mView.getVisibility());

        mView.enableShowOnlyOnFocus(true);
        assertEquals(View.VISIBLE, mView.getVisibility());
    }

    @Test
    public void showOnlyOnFocusButton_hoverChanged() {
        mView.enableShowOnlyOnFocus(true);
        assertEquals(View.INVISIBLE, mView.getVisibility());

        // Button is visible when parent view is hovered.
        mView.onParentViewHoverChanged(true);
        assertEquals(View.VISIBLE, mView.getVisibility());

        // Button is not visible when parent view is not hovered.
        mView.onParentViewHoverChanged(false);
        assertEquals(View.INVISIBLE, mView.getVisibility());

        // Button is visible when button view is hovered.
        mView.setHovered(true);
        assertEquals(View.VISIBLE, mView.getVisibility());

        // Button is not visible when button view is not hovered.
        mView.setHovered(false);
        assertEquals(View.INVISIBLE, mView.getVisibility());
    }
}
