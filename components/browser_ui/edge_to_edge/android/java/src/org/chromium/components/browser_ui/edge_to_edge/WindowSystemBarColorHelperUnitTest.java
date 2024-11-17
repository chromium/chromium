// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.annotation.SuppressLint;
import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.Window;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit test for {@link WindowSystemBarColorHelper} */
@RunWith(BaseRobolectricTestRunner.class)
public class WindowSystemBarColorHelperUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Window mWindow;
    @Mock View mDecorView;

    private int mWindowStatusBarColor;
    private int mWindowNavBarColor;
    private int mWindowNavBarDividerColor;

    @Before
    public void setup() {
        doReturn(mDecorView).when(mWindow).getDecorView();

        doAnswer(invocationOnMock -> mWindowStatusBarColor = invocationOnMock.getArgument(0))
                .when(mWindow)
                .setStatusBarColor(anyInt());
        doAnswer((inv) -> mWindowStatusBarColor).when(mWindow).getStatusBarColor();

        doAnswer(invocationOnMock -> mWindowNavBarColor = invocationOnMock.getArgument(0))
                .when(mWindow)
                .setNavigationBarColor(anyInt());
        doAnswer((inv) -> mWindowNavBarColor).when(mWindow).getNavigationBarColor();

        if (Build.VERSION.SDK_INT >= 28) {
            doAnswer(
                            invocationOnMock ->
                                    mWindowNavBarDividerColor = invocationOnMock.getArgument(0))
                    .when(mWindow)
                    .setNavigationBarDividerColor(anyInt());
            doAnswer((inv) -> mWindowNavBarDividerColor)
                    .when(mWindow)
                    .getNavigationBarDividerColor();
        }
    }

    @Test
    @Config(sdk = 28)
    public void testInitialValue() {
        mWindowStatusBarColor = Color.RED;
        mWindowNavBarColor = Color.YELLOW;
        mWindowNavBarDividerColor = Color.BLACK;

        doReturn(mWindowStatusBarColor).when(mWindow).getStatusBarColor();
        doReturn(mWindowNavBarColor).when(mWindow).getNavigationBarColor();
        doReturn(mWindowNavBarDividerColor).when(mWindow).getNavigationBarDividerColor();

        WindowSystemBarColorHelper helper = new WindowSystemBarColorHelper(mWindow);
        assertEquals(
                "getStatusBarColor is wrong.", mWindowStatusBarColor, helper.getStatusBarColor());
        assertEquals(
                "getNavigationBarColor is wrong.",
                mWindowNavBarColor,
                helper.getNavigationBarColor());
        assertEquals(
                "getNavigationBarDividerColor is wrong.",
                mWindowNavBarDividerColor,
                helper.getNavigationBarDividerColor());
    }

    @Test
    @Config(sdk = 27)
    public void testInitialValue_LowerSdk() {
        mWindowStatusBarColor = Color.RED;
        mWindowNavBarColor = Color.YELLOW;

        WindowSystemBarColorHelper helper = new WindowSystemBarColorHelper(mWindow);
        assertEquals(
                "getStatusBarColor is wrong.", mWindowStatusBarColor, helper.getStatusBarColor());
        assertEquals(
                "getNavigationBarColor is wrong.",
                mWindowNavBarColor,
                helper.getNavigationBarColor());
        assertEquals(
                "getNavigationBarDividerColor is transparent on unsupported sdk.",
                Color.TRANSPARENT,
                helper.getNavigationBarDividerColor());
    }

    @Test
    public void testSetStatusBarColor() {
        WindowSystemBarColorHelper helper = new WindowSystemBarColorHelper(mWindow);
        assertEquals("getStatusBarColor is wrong.", Color.TRANSPARENT, helper.getStatusBarColor());

        int mWindowStatusBarColor = Color.RED;
        helper.setStatusBarColor(mWindowStatusBarColor);
        verify(mWindow, times(1)).setStatusBarColor(mWindowStatusBarColor);
        assertEquals(
                "getStatusBarColor is wrong.", mWindowStatusBarColor, helper.getStatusBarColor());
        verify(mDecorView).setSystemUiVisibility(anyInt());

        // Setting the same color will be ignored.
        helper.setStatusBarColor(mWindowStatusBarColor);
        verify(mWindow, times(1)).setStatusBarColor(mWindowStatusBarColor);
    }

    @Test
    public void testSetNavigationBarColor() {
        WindowSystemBarColorHelper helper = new WindowSystemBarColorHelper(mWindow);
        assertEquals(
                "getStatusBarColor is wrong.", Color.TRANSPARENT, helper.getNavigationBarColor());

        int newNavColor = Color.YELLOW;
        helper.setNavigationBarColor(newNavColor);
        verify(mWindow, times(1)).setNavigationBarColor(newNavColor);
        assertEquals("getStatusBarColor is wrong.", newNavColor, helper.getNavigationBarColor());
        verify(mDecorView).setSystemUiVisibility(anyInt());

        // Setting the same color will be ignored.
        helper.setNavigationBarColor(newNavColor);
        verify(mWindow, times(1)).setNavigationBarColor(newNavColor);
    }

    @Test
    @Config(sdk = 28)
    @SuppressLint("NewApi") // Ignore warning for setNavigationBarDividerColor
    public void testSetNavigationBarDividerColor() {
        WindowSystemBarColorHelper helper = new WindowSystemBarColorHelper(mWindow);
        assertEquals(
                "getNavigationBarDividerColor is transparent on unsupported sdk.",
                Color.TRANSPARENT,
                helper.getNavigationBarDividerColor());

        int newDividerColor = Color.BLACK;
        helper.setNavigationBarDividerColor(newDividerColor);
        verify(mWindow, times(1)).setNavigationBarDividerColor(newDividerColor);
        assertEquals(
                "getStatusBarColor is wrong.",
                newDividerColor,
                helper.getNavigationBarDividerColor());
        // Setting the same color will be ignored.
        helper.setNavigationBarColor(newDividerColor);
        verify(mWindow, times(1)).setNavigationBarDividerColor(newDividerColor);
    }

    @Test
    @Config(sdk = 27)
    public void testSetDividerColorIgnoredOnLowerVersion() {
        WindowSystemBarColorHelper helper = new WindowSystemBarColorHelper(mWindow);
        assertEquals(
                "getNavigationBarDividerColor is transparent on unsupported sdk.",
                Color.TRANSPARENT,
                helper.getNavigationBarDividerColor());

        int newDividerColor = Color.BLACK;
        helper.setNavigationBarDividerColor(newDividerColor);
        assertEquals(
                "getStatusBarColor is wrong.",
                Color.TRANSPARENT,
                helper.getNavigationBarDividerColor());
    }

    @Test
    @Config(sdk = 29)
    @SuppressLint("NewApi") // Ignore warning for setNavigationBarContrastEnforced.
    public void testSetNavigationBarContrastEnforced() {
        WindowSystemBarColorHelper helper = new WindowSystemBarColorHelper(mWindow);

        helper.setNavigationBarContrastEnforced(true);
        verify(mWindow).setNavigationBarContrastEnforced(true);

        helper.setNavigationBarContrastEnforced(false);
        verify(mWindow).setNavigationBarContrastEnforced(false);
    }
}
