// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Rect;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class WebAppHeaderLayoutMediatorTest {
    private static final int SCREEN_WIDTH = 800;
    private static final int SCREEN_HEIGHT = 1600;
    private static final int SYS_APP_HEADER_HEIGHT = 40;
    private static final int LEFT_INSET = 50;
    private static final int RIGHT_INSET = 60;
    private static final Rect WIDEST_UNOCCLUDED_RECT =
            new Rect(LEFT_INSET, 0, SCREEN_WIDTH - RIGHT_INSET, SYS_APP_HEADER_HEIGHT);

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    private WebAppHeaderLayoutMediator mMediator;
    private PropertyModel mModel;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    @Mock public DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock public Tab mTab;

    @Before
    public void setup() {
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(null);

        mTabSupplier = new ObservableSupplierImpl<>();
        mModel = new PropertyModel.Builder(WebAppHeaderLayoutProperties.ALL_KEYS).build();
        mMediator =
                new WebAppHeaderLayoutMediator(
                        mModel, mDesktopWindowStateManager, mTabSupplier, SYS_APP_HEADER_HEIGHT);
    }

    @Test
    public void testHasAppHeaderStateOnInit_setPaddingsMatchingInsets() {
        final AppHeaderState appHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), WIDEST_UNOCCLUDED_RECT, true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        mMediator =
                new WebAppHeaderLayoutMediator(
                        mModel, mDesktopWindowStateManager, mTabSupplier, SYS_APP_HEADER_HEIGHT);

        assertEquals(
                "Header min height should match app header height",
                SYS_APP_HEADER_HEIGHT,
                mModel.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        assertEquals(
                "Header paddings should match system insets",
                new Rect(LEFT_INSET, 0, RIGHT_INSET, 0),
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
        assertTrue(
                "Header view should be visible",
                mModel.get(WebAppHeaderLayoutProperties.IS_VISIBLE));
    }

    @Test
    public void testAppHeaderStateUpdated_setPaddingsMatchingInsets() {
        final AppHeaderState appHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), WIDEST_UNOCCLUDED_RECT, true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        mMediator.onAppHeaderStateChanged(appHeaderState);
        assertEquals(
                "Header min height should match app header height",
                SYS_APP_HEADER_HEIGHT,
                mModel.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        assertEquals(
                "Header paddings should match updated system insets",
                new Rect(LEFT_INSET, 0, RIGHT_INSET, 0),
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
        assertTrue(
                "Header view should be visible",
                mModel.get(WebAppHeaderLayoutProperties.IS_VISIBLE));
    }

    @Test
    public void testNotInDesktopWindow_hideHeader() {
        final AppHeaderState appHeaderState =
                new AppHeaderState(new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), new Rect(), false);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        WebAppHeaderLayoutMediator.setMinHeightForTesting(SYS_APP_HEADER_HEIGHT);

        final Rect initialPaddings = new Rect(0, 0, 0, 0);
        mModel.set(WebAppHeaderLayoutProperties.PADDINGS, initialPaddings);

        mMediator.onAppHeaderStateChanged(appHeaderState);
        assertEquals(
                "Header min height should match default min height",
                SYS_APP_HEADER_HEIGHT,
                mModel.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        assertEquals(
                "Header paddings should match initial view paddings",
                initialPaddings,
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
        assertFalse(
                "Header view should be gone", mModel.get(WebAppHeaderLayoutProperties.IS_VISIBLE));
    }

    @Test
    public void testAppHeaderHeightIsLessThanMin_noTopPaddingsSet() {
        final AppHeaderState appHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), WIDEST_UNOCCLUDED_RECT, true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        mMediator.onAppHeaderStateChanged(appHeaderState);
        assertEquals(
                "Header min height should match default min height",
                SYS_APP_HEADER_HEIGHT,
                mModel.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        assertEquals(
                "Vertical paddings should be 0 when system bar is less than min height",
                new Rect(LEFT_INSET, 0, RIGHT_INSET, 0),
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
    }

    @Test
    public void testAppHeaderHeightIsGreaterThanMin_setTopPaddingEqualExceedingSize() {
        final int headerHeight = SYS_APP_HEADER_HEIGHT + 10;
        final Rect widestUnoccludedRect =
                new Rect(LEFT_INSET, 0, SCREEN_WIDTH - RIGHT_INSET, headerHeight);
        final AppHeaderState appHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), widestUnoccludedRect, true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        WebAppHeaderLayoutMediator.setMinHeightForTesting(SYS_APP_HEADER_HEIGHT);

        mMediator.onAppHeaderStateChanged(appHeaderState);
        assertEquals(
                "Header min height should match app header height",
                headerHeight,
                mModel.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        assertEquals(
                "Top padding should match exceeding size of the app header",
                new Rect(LEFT_INSET, 10, RIGHT_INSET, 0),
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
    }

    @Test
    public void testGoBackWithHistory_shouldGoBack() {
        mTabSupplier.set(mTab);
        when(mTab.canGoBack()).thenReturn(true);

        mMediator.goBack();
        verify(mTab).goBack();
    }

    @Test
    public void testGoBackNoHistory_shouldNotGoBack() {
        mTabSupplier.set(mTab);
        when(mTab.canGoBack()).thenReturn(false);

        mMediator.goBack();
        verify(mTab, never()).goBack();
    }

    @Test
    public void testGoBackNoTab_shouldNotGoBack() {
        mMediator.goBack();
        verify(mTab, never()).goBack();
    }
}
