// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.edge_to_edge.layout.EdgeToEdgeLayoutCoordinator;
import org.chromium.ui.util.AttrUtils;

/** Unit tests for {@link ChromeDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeDialogUnitTest {
    private Activity mActivity;
    private ChromeDialog mDialog;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.getTheme().applyStyle(R.style.Theme_BrowserUI_DayNight, true);
    }

    @Test
    public void testFullscreenDialog() {
        mDialog =
                new ChromeDialog(
                        mActivity,
                        R.style.ThemeOverlay_BrowserUI_Fullscreen,
                        /* shouldPadForWindowInsets= */ true);
        mDialog.setContentView(new View(mActivity));
        EdgeToEdgeLayoutCoordinator edgeToEdgeLayoutCoordinator =
                mDialog.getEdgeToEdgeLayoutCoordinatorForTesting();

        Assert.assertNotNull(
                "EdgeToEdgeCoordinator should not be null", edgeToEdgeLayoutCoordinator);
        assertEquals(
                "Status bar colors is incorrect",
                AttrUtils.resolveColor(
                        mDialog.getContext().getTheme(), android.R.attr.statusBarColor),
                edgeToEdgeLayoutCoordinator.getStatusBarColor());
        assertEquals(
                "Navigation bar colors is incorrect",
                AttrUtils.resolveColor(
                        mDialog.getContext().getTheme(), android.R.attr.navigationBarColor),
                edgeToEdgeLayoutCoordinator.getNavigationBarColor());
        assertEquals(
                "Navigation bar divider colors is incorrect",
                AttrUtils.resolveColor(
                        mDialog.getContext().getTheme(), android.R.attr.navigationBarDividerColor),
                edgeToEdgeLayoutCoordinator.getNavigationBarDividerColor());
    }

    @Test
    public void testNonFullscreenDialog_NoEdgeToEdgeCoordinator() {
        mDialog = new ChromeDialog(mActivity, 0, /* shouldPadForWindowInsets= */ true);
        mDialog.setContentView(new View(mActivity));
        Assert.assertNull(
                "EdgeToEdgeCoordinator should be null for non-fullscreen dialog.",
                mDialog.getEdgeToEdgeLayoutCoordinatorForTesting());
    }

    @Test
    public void testShouldPadForWindowInsetsFalse_NoEdgeToEdgeCoordinator() {
        mDialog =
                new ChromeDialog(
                        mActivity,
                        R.style.ThemeOverlay_BrowserUI_Fullscreen,
                        /* shouldPadForWindowInsets= */ false);
        mDialog.setContentView(new View(mActivity));
        Assert.assertNull(
                "EdgeToEdgeCoordinator should be null when shouldPadForWindowInsets is false.",
                mDialog.getEdgeToEdgeLayoutCoordinatorForTesting());
    }

    @Test
    public void testSetNavBarColor_Fullscreen() {
        mDialog =
                new ChromeDialog(
                        mActivity,
                        R.style.ThemeOverlay_BrowserUI_Fullscreen,
                        /* shouldPadForWindowInsets= */ true);
        mDialog.setContentView(new View(mActivity));
        var e2eCoordinator = mDialog.getEdgeToEdgeLayoutCoordinatorForTesting();

        int testColor = Color.CYAN;
        mDialog.setNavBarColor(testColor);
        assertEquals(
                "Navigation bar color should be updated correctly for fullscreen dialog.",
                testColor,
                e2eCoordinator.getNavigationBarColor());
    }

    @Test
    public void testSetNavBarColor_NonFullscreen() {
        mDialog = new ChromeDialog(mActivity, 0, /* shouldPadForWindowInsets= */ true);
        mDialog.setContentView(new View(mActivity));

        int testColor = Color.CYAN;
        mDialog.setNavBarColor(testColor);
        assertEquals(
                "Navigation bar color should be updated correctly for non-fullscreen dialog.",
                testColor,
                mDialog.getWindow().getNavigationBarColor());
    }
}
