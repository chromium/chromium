// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.Y_OFFSET;

import android.graphics.Color;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EdgeToEdgeBottomChinMediatorTest {
    @Mock private LayoutManager mLayoutManager;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private NavigationBarColorProvider mNavigationBarColorProvider;
    @Mock private BottomControlsStacker mBottomControlsStacker;

    private PropertyModel mModel;
    private EdgeToEdgeBottomChinMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel.Builder(EdgeToEdgeBottomChinProperties.ALL_KEYS).build();
        mMediator =
                new EdgeToEdgeBottomChinMediator(
                        mModel,
                        mLayoutManager,
                        mEdgeToEdgeController,
                        mNavigationBarColorProvider,
                        mBottomControlsStacker);
    }

    @Test
    public void testInitialization() {
        assertEquals(0, mModel.get(Y_OFFSET));

        verify(mLayoutManager).addObserver(eq(mMediator));
        verify(mEdgeToEdgeController).registerObserver(eq(mMediator));
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();

        verify(mLayoutManager).removeObserver(eq(mMediator));
        verify(mEdgeToEdgeController).unregisterObserver(eq(mMediator));
    }

    @Test
    public void testUpdateHeight() {
        mMediator.onToEdgeChange(60);
        assertEquals(
                "The height should have adjusted to 60 to match the edge-to-edge bottom inset.",
                60,
                mModel.get(HEIGHT));

        mMediator.onToEdgeChange(0);
        assertEquals(
                "The height should have been cleared to 0 to match the edge-to-edge bottom inset.",
                0,
                mModel.get(HEIGHT));
    }

    @Test
    public void testUpdateColor() {
        mMediator.onNavigationBarColorChanged(Color.BLUE);
        assertEquals("The color should have been updated to blue.", Color.BLUE, mModel.get(COLOR));

        mMediator.onNavigationBarColorChanged(Color.RED);
        assertEquals("The color should have been updated to red.", Color.RED, mModel.get(COLOR));
    }

    @Test
    public void testUpdateVisibility() {
        assertFalse(
                "The chin should not be visible as it has just been initialized.",
                mModel.get(IS_VISIBLE));

        mMediator.onStartedShowing(LayoutType.BROWSING);
        mMediator.onToEdgeChange(0);
        assertFalse(
                "The chin should not be visible as the edge-to-edge bottom inset is still 0.",
                mModel.get(IS_VISIBLE));

        mMediator.onStartedShowing(LayoutType.NONE);
        mMediator.onToEdgeChange(60);
        assertFalse(
                "The chin should not be visible as the layout type does not support showing the"
                        + " chin.",
                mModel.get(IS_VISIBLE));

        mMediator.onStartedShowing(LayoutType.BROWSING);
        mMediator.onToEdgeChange(60);
        assertTrue("The chin should be visible as all conditions are met.", mModel.get(IS_VISIBLE));
    }
}
