// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link ActionButtonView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionButtonViewUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private ActionButtonView mView;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        mView = spy(new ActionButtonView(context));
    }

    @Test
    public void notShowOnlyOnFocusButton() {
        mView.enableShowOnlyOnFocus(false);
        verify(mView).setVisibility(View.VISIBLE);
        verify(mView, times(0)).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);

        mView.onParentViewSelected(true);
        verify(mView).setVisibility(View.VISIBLE);
        verify(mView, times(0)).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);

        mView.onParentViewSelected(false);
        verify(mView).setVisibility(View.VISIBLE);
        verify(mView, times(0)).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);

        mView.onParentViewHoverChanged(true);
        verify(mView).setVisibility(View.VISIBLE);
        verify(mView, times(0)).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);

        mView.onParentViewHoverChanged(false);
        verify(mView).setVisibility(View.VISIBLE);
        verify(mView, times(0)).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);
    }

    @Test
    public void showOnlyOnFocusButton_selected() {
        mView.enableShowOnlyOnFocus(true);
        verify(mView).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.VISIBLE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);

        mView.onParentViewSelected(true);
        verify(mView).setVisibility(View.VISIBLE);
        verify(mView).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);

        mView.onParentViewSelected(false);
        verify(mView).setVisibility(View.VISIBLE);
        verify(mView, times(2)).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);
    }

    @Test
    public void showOnlyOnFocusButton_hoverChanged() {
        mView.enableShowOnlyOnFocus(true);
        verify(mView).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.VISIBLE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);

        // Button is visible when parent view is hovered.
        mView.onParentViewHoverChanged(true);
        verify(mView).setVisibility(View.VISIBLE);
        verify(mView).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);

        // Button is not visible when parent view is not hovered.
        mView.onParentViewHoverChanged(false);
        verify(mView).setVisibility(View.VISIBLE);
        verify(mView, times(2)).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);

        // Button is visible when button view is hovered.
        mView.setHovered(true);
        verify(mView, times(2)).setVisibility(View.VISIBLE);
        verify(mView, times(2)).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);

        // Button is not visible when button view is not hovered.
        mView.setHovered(false);
        verify(mView, times(2)).setVisibility(View.VISIBLE);
        verify(mView, times(3)).setVisibility(View.GONE);
        verify(mView, times(0)).setVisibility(View.INVISIBLE);
    }
}
