// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

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

/** Instrumentation tests for PWA Universal Install bottom sheet. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class PwaUniversalInstallBottomSheetCoordinatorTest {
    Activity mActivity;

    @Mock private BottomSheetController mBottomSheetControllerMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @MediumTest
    public void testShowing() {
        final Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        PwaUniversalInstallBottomSheetCoordinator coordinator =
                new PwaUniversalInstallBottomSheetCoordinator(activity, mBottomSheetControllerMock);

        View bottomSheetView = coordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TextView title = bottomSheetView.findViewById(R.id.title);
                    String expected = "Add to home screen";
                    Assert.assertEquals(expected, title.getText());
                });
    }
}
