// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.widget.TextView;

import androidx.appcompat.widget.Toolbar;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for ToolbarUtils. */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarUtilsTest {
    @Test
    public void getTitleTextView() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(), R.style.Theme_AppCompat_Light);
        Toolbar toolbar = new Toolbar(context);
        assertNull(ToolbarUtils.getTitleTextView(toolbar));

        // TitleView is instantiated only if the title is set.
        toolbar.setTitle("Demon Hunters");
        assertNotNull(ToolbarUtils.getTitleTextView(toolbar));

        // Adds another TextView to see if the method returns the right one.
        TextView subTextView = new TextView(context);
        subTextView.setText("Demon Hunters");
        toolbar.addView(subTextView);

        // Verify that the returned view is not the non-title TextView added above.
        TextView titleView = ToolbarUtils.getTitleTextView(toolbar);
        assertNotNull(titleView);
        assertNotSame(titleView, subTextView);
    }
}
