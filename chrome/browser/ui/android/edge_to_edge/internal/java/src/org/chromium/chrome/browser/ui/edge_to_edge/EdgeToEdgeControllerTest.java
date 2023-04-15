// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests the EdgeToEdgeController code. Ideally this would include {@link EdgeToEdgeController},
 *  {@link EdgeToEdgeControllerFactory},  along with {@link EdgeToEdgeControllerImpl}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EdgeToEdgeControllerTest {
    EdgeToEdgeControllerImpl mEdgeToEdgeControllerImpl;

    @Before
    public void setUp() {
        var activity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mEdgeToEdgeControllerImpl =
                (EdgeToEdgeControllerImpl) EdgeToEdgeControllerFactory.create(activity);
        Assert.assertNotNull(mEdgeToEdgeControllerImpl);
    }

    @Test
    public void drawUnderSystemBars_basic() {
        mEdgeToEdgeControllerImpl.drawUnderSystemBars();
    }

    @Test
    public void drawUnderSystemBarsInternal_assertsBadId() {
        final int badId = 1234;
        Assert.assertThrows(
                AssertionError.class, () -> mEdgeToEdgeControllerImpl.drawUnderSystemBars(badId));
    }

    // TODO: Verify inset or drawn under
    // TODO: Test with a different SDK
}
