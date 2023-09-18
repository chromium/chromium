// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.webapps.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Instrumentation tests for PWA Restore bottom sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class PwaRestoreBottomSheetCoordinatorTest {
    Activity mActivity;

    @Mock
    private BottomSheetController mBottomSheetControllerMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @MediumTest
    public void testShowing() {
        final Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        PwaRestoreBottomSheetCoordinator coordinator =
                new PwaRestoreBottomSheetCoordinator(activity, mBottomSheetControllerMock);

        View bottomSheetView = coordinator.getBottomSheetToolbarViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView title = bottomSheetView.findViewById(R.id.title);
            String expected = "Restore your web apps";
            Assert.assertEquals(expected, title.getText());

            TextView description = bottomSheetView.findViewById(R.id.description);
            expected =
                    "Restore web apps you have recently used on devices connected to this account";
            Assert.assertEquals(expected, description.getText());

            View button = bottomSheetView.findViewById(R.id.button);
            Assert.assertTrue(button.isEnabled());
        });
    }
}
