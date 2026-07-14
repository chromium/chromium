// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.displaystyle;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;

/** Unit tests for {@link UiConfig}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UiConfigTest {
    private View mView;
    private UiConfig mUiConfig;

    @Before
    public void setUp() {
        mView = new View(RuntimeEnvironment.getApplication());
    }

    @Test
    @Config(qualifiers = "w300dp-h600dp")
    public void testDisplayStyle_Narrow() {
        mUiConfig = new UiConfig(mView);
        assertEquals(HorizontalDisplayStyle.NARROW, mUiConfig.getCurrentDisplayStyle().horizontal);
        assertEquals(VerticalDisplayStyle.REGULAR, mUiConfig.getCurrentDisplayStyle().vertical);
    }

    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testDisplayStyle_Regular() {
        mUiConfig = new UiConfig(mView);
        assertEquals(HorizontalDisplayStyle.REGULAR, mUiConfig.getCurrentDisplayStyle().horizontal);
        assertEquals(VerticalDisplayStyle.REGULAR, mUiConfig.getCurrentDisplayStyle().vertical);
    }

    @Test
    @Config(qualifiers = "w700dp-h600dp")
    public void testDisplayStyle_Wide() {
        mUiConfig = new UiConfig(mView);
        assertEquals(HorizontalDisplayStyle.WIDE, mUiConfig.getCurrentDisplayStyle().horizontal);
        assertEquals(VerticalDisplayStyle.REGULAR, mUiConfig.getCurrentDisplayStyle().vertical);
    }

    @Test
    @Config(qualifiers = "w600dp-h300dp")
    public void testDisplayStyle_Flat() {
        mUiConfig = new UiConfig(mView);
        assertEquals(VerticalDisplayStyle.FLAT, mUiConfig.getCurrentDisplayStyle().vertical);
    }

    @Test
    @Config(qualifiers = "w700dp-h600dp")
    public void testSetHorizontalInset() {
        mUiConfig = new UiConfig(mView);
        assertEquals(HorizontalDisplayStyle.WIDE, mUiConfig.getCurrentDisplayStyle().horizontal);

        // Apply inset that brings effective width to 500dp (700 - 200).
        mUiConfig.setHorizontalInset(200);
        assertEquals(HorizontalDisplayStyle.REGULAR, mUiConfig.getCurrentDisplayStyle().horizontal);

        // Apply inset that brings effective width to 300dp (700 - 400).
        mUiConfig.setHorizontalInset(400);
        assertEquals(HorizontalDisplayStyle.NARROW, mUiConfig.getCurrentDisplayStyle().horizontal);
    }

    @Test
    @Config(qualifiers = "w700dp-h600dp")
    public void testObserverNotification() {
        mUiConfig = new UiConfig(mView);
        DisplayStyleObserver observer = mock(DisplayStyleObserver.class);
        mUiConfig.addObserver(observer);

        // Initial notification upon adding observer.
        verify(observer)
                .onDisplayStyleChanged(
                        new DisplayStyle(
                                HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR));

        // Update with inset to trigger change (700 - 200 = 500 -> Regular).
        mUiConfig.setHorizontalInset(200);
        verify(observer)
                .onDisplayStyleChanged(
                        new DisplayStyle(
                                HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR));
    }
}
