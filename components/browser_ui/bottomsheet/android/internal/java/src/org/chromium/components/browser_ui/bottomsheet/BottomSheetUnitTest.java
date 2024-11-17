// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link BottomSheet}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomSheetUnitTest {
    private static final int APP_HEADER_HEIGHT = 42;

    @Mock private ViewGroup mSheetContainer;
    @Mock private MarginLayoutParams mSheetLayoutParams;

    @Captor ArgumentCaptor<MarginLayoutParams> mMarginCaptor;

    private BottomSheet mBottomSheet;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        Activity activity = buildActivity(Activity.class).setup().get();
        mBottomSheet =
                (BottomSheet) LayoutInflater.from(activity).inflate(R.layout.bottom_sheet, null);
        mBottomSheet.setSheetContainerForTesting(mSheetContainer);
        when(mSheetContainer.getLayoutParams()).thenReturn(mSheetLayoutParams);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SwitchToDesktopWindow() {
        // Simulate switching to desktop windowing mode.
        mBottomSheet.onAppHeaderHeightChanged(APP_HEADER_HEIGHT);
        verify(mSheetContainer).setLayoutParams(mMarginCaptor.capture());
        assertEquals(
                "Sheet container's top margin should be updated to account for app header height.",
                APP_HEADER_HEIGHT,
                mMarginCaptor.getValue().topMargin);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SwitchOutOfDesktopWindow() {
        // Sheet container in desktop window will use top margin = APP_HEADER_HEIGHT.
        mSheetLayoutParams.topMargin = APP_HEADER_HEIGHT;
        // Simulate switching out of desktop windowing mode.
        mBottomSheet.onAppHeaderHeightChanged(0);
        verify(mSheetContainer).setLayoutParams(mMarginCaptor.capture());
        assertEquals(
                "Sheet container's top margin should be reset.",
                0,
                mMarginCaptor.getValue().topMargin);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SameAsContainerTopMargin() {
        mSheetLayoutParams.topMargin = APP_HEADER_HEIGHT;
        mBottomSheet.onAppHeaderHeightChanged(APP_HEADER_HEIGHT);
        // Avoid LayoutParams update when there's no change in the top margin.
        verify(mSheetContainer, never()).setLayoutParams(any());
    }
}
