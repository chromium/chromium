// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.autofill.internal.R;

/** View tests for AtMemoryFlyoutView. */
@RunWith(BaseRobolectricTestRunner.class)
public class AtMemoryFlyoutViewTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testBackClickNotifiesCallback() {
        AtMemoryFlyoutView view = new AtMemoryFlyoutView(mContext);
        Runnable mockCallback = mock(Runnable.class);
        view.setBackButtonCallback(mockCallback);

        View backButton = view.getContentView().findViewById(R.id.flyout_back_button);
        backButton.performClick();

        verify(mockCallback).run();
    }

    @Test
    public void testSourceClickNotifiesCallback() {
        AtMemoryFlyoutView view = new AtMemoryFlyoutView(mContext);
        Runnable mockCallback = mock(Runnable.class);
        view.setSourceClickCallback(mockCallback);

        View sourceButton = view.getContentView().findViewById(R.id.flyout_source_button);
        sourceButton.performClick();

        verify(mockCallback).run();
    }

    @Test
    public void testManageClickNotifiesCallback() {
        AtMemoryFlyoutView view = new AtMemoryFlyoutView(mContext);
        Runnable mockCallback = mock(Runnable.class);
        view.setManageClickCallback(mockCallback);

        View manageButton = view.getContentView().findViewById(R.id.flyout_manage_button);
        manageButton.performClick();

        verify(mockCallback).run();
    }
}
