// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.webapps.R;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreProperties.ViewState;
import org.chromium.ui.shadows.ShadowColorUtils;

import java.util.ArrayList;
import java.util.List;

/** Instrumentation tests for PWA Restore bottom sheet. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowColorUtils.class})
@LooperMode(LooperMode.Mode.PAUSED)
public class PwaRestoreBottomSheetCoordinatorTest {
    Activity mActivity;

    @Rule public JniMocker mocker = new JniMocker();

    // Each entry in this list should have a corresponding entry in
    // mLastUsedList below.
    private final String[] mDefaultAppIds = new String[] {"appId1", "appId2", "appId3"};
    private final String[] mDefaultAppNames = new String[] {"App 1", "App 2", "App 3"};
    private final ArrayList<Bitmap> mDefaultAppIcons =
            new ArrayList<Bitmap>(
                    List.of(
                            createBitmap(Color.RED),
                            createBitmap(Color.GREEN),
                            createBitmap(Color.BLUE)));
    private final int[] mLastUsedList = new int[] {1, 1, 35};

    @Mock private BottomSheetController mBottomSheetControllerMock;
    @Mock private PwaRestoreBottomSheetMediator.Natives mNativeMediatorMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mocker.mock(PwaRestoreBottomSheetMediatorJni.TEST_HOOKS, mNativeMediatorMock);
        when(mNativeMediatorMock.initialize(Mockito.any())).thenReturn(0L);
    }

    @After
    public void tearDown() {
        ShadowColorUtils.sInNightMode = false;
    }

    private static Bitmap createBitmap(int color) {
        int[] colors = {color};
        return Bitmap.createBitmap(colors, 1, 1, Bitmap.Config.ALPHA_8);
    }

    @Test
    @MediumTest
    public void testViewInitialization() {
        PwaRestoreBottomSheetCoordinator coordinator =
                new PwaRestoreBottomSheetCoordinator(
                        mDefaultAppIds,
                        mDefaultAppNames,
                        mDefaultAppIcons,
                        mLastUsedList,
                        mActivity,
                        mBottomSheetControllerMock,
                        /* backArrowId= */ 0);

        View bottomSheetView = coordinator.getBottomSheetViewForTesting();
        {
            TextView title = bottomSheetView.findViewById(R.id.title_preview);
            String expected = "Restore your web apps";
            Assert.assertEquals(expected, title.getText());

            TextView description = bottomSheetView.findViewById(R.id.description_preview);
            expected =
                    "Restore web apps you have recently used on devices connected to this account";
            Assert.assertEquals(expected, description.getText());

            View button = bottomSheetView.findViewById(R.id.review_button);
            Assert.assertTrue(button.isEnabled());
        }

        {
            TextView title = bottomSheetView.findViewById(R.id.title_content);
            String expected = "Review web apps";
            Assert.assertEquals(expected, title.getText());

            TextView description = bottomSheetView.findViewById(R.id.description_content);
            expected =
                    "Choose web apps to restore on this device. Apps shown here are based on "
                            + "your Chrome history.";
            Assert.assertEquals(expected, description.getText());

            View pwaList = bottomSheetView.findViewById(R.id.pwa_list);
            Assert.assertTrue(pwaList.getVisibility() == View.VISIBLE);

            View deselectButton = bottomSheetView.findViewById(R.id.deselect_button);
            Assert.assertTrue(deselectButton.isEnabled());

            View restoreButton = bottomSheetView.findViewById(R.id.restore_button);
            Assert.assertTrue(restoreButton.isEnabled());
        }
    }

    @Test
    @MediumTest
    public void testShowAndExpand() {
        PwaRestoreBottomSheetCoordinator coordinator =
                new PwaRestoreBottomSheetCoordinator(
                        mDefaultAppIds,
                        mDefaultAppNames,
                        mDefaultAppIcons,
                        mLastUsedList,
                        mActivity,
                        mBottomSheetControllerMock,
                        /* backArrowId= */ 0);

        coordinator.show();

        // Calling show() results in the bottom sheet showing (peeking).
        Assert.assertEquals(
                ViewState.PREVIEW,
                coordinator.getModelForTesting().get(PwaRestoreProperties.VIEW_STATE));
        verify(mBottomSheetControllerMock, times(1)).requestShowContent(any(), eq(true));

        coordinator.onReviewButtonClicked();

        // Clicking the Review button results in the sheet expanding.
        Assert.assertEquals(
                ViewState.VIEW_PWA_LIST,
                coordinator.getModelForTesting().get(PwaRestoreProperties.VIEW_STATE));

        coordinator.onDialogBackButtonClicked();

        // Clicking the Dialog Back button results in the sheet going back to peeking state.
        Assert.assertEquals(
                ViewState.PREVIEW,
                coordinator.getModelForTesting().get(PwaRestoreProperties.VIEW_STATE));

        coordinator.onReviewButtonClicked();

        // Clicking the Review button (again) results in the sheet expanding.
        Assert.assertEquals(
                ViewState.VIEW_PWA_LIST,
                coordinator.getModelForTesting().get(PwaRestoreProperties.VIEW_STATE));

        coordinator.onOsBackButtonClicked();

        // Clicking the OS Back button results in the sheet going back to peeking state.
        Assert.assertEquals(
                ViewState.PREVIEW,
                coordinator.getModelForTesting().get(PwaRestoreProperties.VIEW_STATE));
    }
}
