// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.webapps.R;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreProperties.ViewState;
import org.chromium.ui.shadows.ShadowColorUtils;

/**
 * Instrumentation tests for PWA Restore bottom sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowColorUtils.class})
@LooperMode(LooperMode.Mode.PAUSED)
public class PwaRestoreBottomSheetCoordinatorTest {
    Activity mActivity;

    @Mock
    private BottomSheetController mBottomSheetControllerMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
    }

    @After
    public void tearDown() {
        ShadowColorUtils.sInNightMode = false;
    }

    @Test
    @MediumTest
    public void testViewInitialization() {
        PwaRestoreBottomSheetCoordinator coordinator = new PwaRestoreBottomSheetCoordinator(
                mActivity, mBottomSheetControllerMock, /* backArrowId= */ 0);

        View bottomSheetView = coordinator.getBottomSheetToolbarViewForTesting();
        {
            TextView title = bottomSheetView.findViewById(R.id.title);
            String expected = "Restore your web apps";
            Assert.assertEquals(expected, title.getText());

            TextView description = bottomSheetView.findViewById(R.id.description);
            expected =
                    "Restore web apps you have recently used on devices connected to this account";
            Assert.assertEquals(expected, description.getText());

            View button = bottomSheetView.findViewById(R.id.review_button);
            Assert.assertTrue(button.isEnabled());
        }

        View contentSheetView = coordinator.getBottomSheetContentViewForTesting();
        {
            TextView title = contentSheetView.findViewById(R.id.title);
            String expected = "Review web apps";
            Assert.assertEquals(expected, title.getText());

            TextView description = contentSheetView.findViewById(R.id.description);
            expected = "Choose web apps to restore on this device. Apps shown here are based on "
                    + "your Chrome history.";
            Assert.assertEquals(expected, description.getText());

            View button = contentSheetView.findViewById(R.id.restore_button);
            Assert.assertTrue(button.isEnabled());
        }
    }

    @Test
    @MediumTest
    public void testShowAndExpand() {
        PwaRestoreBottomSheetCoordinator coordinator = new PwaRestoreBottomSheetCoordinator(
                mActivity, mBottomSheetControllerMock, /* backArrowId= */ 0);

        coordinator.show();

        // Calling show() results in the bottom sheet showing (peeking).
        Assert.assertEquals(ViewState.PREVIEW,
                coordinator.getModelForTesting().get(PwaRestoreProperties.VIEW_STATE));
        verify(mBottomSheetControllerMock, times(1)).requestShowContent(any(), eq(true));

        coordinator.onReviewButtonClicked();

        // Clicking the Review button results in the sheet expanding.
        Assert.assertEquals(ViewState.VIEW_PWA_LIST,
                coordinator.getModelForTesting().get(PwaRestoreProperties.VIEW_STATE));
        verify(mBottomSheetControllerMock, times(1)).expandSheet();

        coordinator.onBackButtonClicked();

        // Clicking the Back button results in the sheet going back to peeking state.
        Assert.assertEquals(ViewState.PREVIEW,
                coordinator.getModelForTesting().get(PwaRestoreProperties.VIEW_STATE));
        verify(mBottomSheetControllerMock, times(1)).collapseSheet(eq(true));
    }
}
