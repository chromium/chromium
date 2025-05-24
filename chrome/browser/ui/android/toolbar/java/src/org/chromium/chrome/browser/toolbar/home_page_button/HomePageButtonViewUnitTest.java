// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;

/** Unit tests for {@link HomePageButtonView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomePageButtonViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HomePageButtonData mHomePageButtonData;
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private View.OnLongClickListener mOnLongClickListener;

    private HomePageButtonView mHomePageButtonView;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mHomePageButtonView =
                LayoutInflater.from(context)
                        .inflate(R.layout.home_page_buttons_layout, null, false)
                        .findViewById(R.id.home_button);
    }

    @Test
    public void testSetVisibility() {
        mHomePageButtonView.setVisibility(true);
        assertEquals(View.VISIBLE, mHomePageButtonView.getVisibility());

        mHomePageButtonView.setVisibility(false);
        assertEquals(View.GONE, mHomePageButtonView.getVisibility());
    }

    @Test
    public void testUpdateButtonData() {
        // Verifies both the OnClickListener and OnLongClickListener are updated correctly.
        when(mHomePageButtonData.getOnClickListener()).thenReturn(mOnClickListener);
        when(mHomePageButtonData.getOnLongClickListener()).thenReturn(mOnLongClickListener);
        mHomePageButtonView.updateButtonData(mHomePageButtonData);
        assertNotNull(mHomePageButtonView);
        mHomePageButtonView.performClick();
        verify(mOnClickListener).onClick(any());
        mHomePageButtonView.performLongClick();
        verify(mOnLongClickListener).onLongClick(any());

        // Verifies when the updated OnLongClickListener is null, the button is not long clickable.
        when(mHomePageButtonData.getOnLongClickListener()).thenReturn(null);
        mHomePageButtonView.updateButtonData(mHomePageButtonData);
        assertFalse(mHomePageButtonView.isLongClickable());
    }
}
