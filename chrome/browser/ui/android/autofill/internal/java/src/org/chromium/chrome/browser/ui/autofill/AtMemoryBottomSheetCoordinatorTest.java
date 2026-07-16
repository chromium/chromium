// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.personal_context.first_run.PersonalContextFirstRunService;
import org.chromium.chrome.browser.personal_context.first_run.PersonalContextFirstRunServiceJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/** Unit tests for {@link AtMemoryBottomSheetCoordinator}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class AtMemoryBottomSheetCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AtMemoryBottomSheetCoordinator.Delegate mMockDelegate;
    @Mock private Profile mProfile;
    @Mock private PersonalContextFirstRunService.Natives mFirstRunServiceJniMock;

    private AtMemoryBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        PersonalContextFirstRunServiceJni.setInstanceForTesting(mFirstRunServiceJniMock);
        mCoordinator =
                new AtMemoryBottomSheetCoordinator(
                        new ContextThemeWrapper(
                                ApplicationProvider.getApplicationContext(),
                                R.style.Theme_BrowserUI_DayNight),
                        mBottomSheetController,
                        mMockDelegate,
                        mProfile);
    }

    @Test
    public void testInitialization() {
        assertNotNull(mCoordinator);
    }

    @Test
    public void testShow_Success() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);

        mCoordinator.show(List.of());

        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }

    @Test
    public void testShow_Failed() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(false);

        mCoordinator.show(List.of());

        verify(mMockDelegate).onDismissed();
    }

    @Test
    public void testHide() {
        mCoordinator.hide();

        verify(mBottomSheetController).hideContent(any(), eq(true));
    }

    @Test
    public void testShow_FocusSearchArea() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);

        mCoordinator.show(List.of());

        View contentView = mCoordinator.getBottomSheetContentForTesting().getContentView();
        View searchInput = contentView.findViewById(R.id.search_query_input);
        assertNotNull(searchInput);
        assertTrue(searchInput.hasFocus());
    }

    @Test
    public void testShow_FlyoutScreen() {
        // TODO(crbug.com/513146609): Implement the test case when the user queries
        // the at.memory search. The list of suggestions are shown and the user clicks
        // the detail page button, then the flyout screen is shown. In that case, the
        // bottom sheet should be updated to show the flyout.
    }

    @Test
    public void testExpand_WhenSheetStateFull_AndNotExpandInHalfHeight() {
        when(mBottomSheetController.getSheetState())
                .thenReturn(BottomSheetController.SheetState.FULL);

        mCoordinator.expand(/* expandInHalfHeight= */ false);

        verify(mBottomSheetController, never()).expandSheet(eq(true));
    }

    @Test
    public void testExpand_WhenSheetStateHalf_AndNotExpandInHalfHeight() {
        when(mBottomSheetController.getSheetState())
                .thenReturn(BottomSheetController.SheetState.HALF);

        mCoordinator.expand(/* expandInHalfHeight= */ false);

        verify(mBottomSheetController).expandSheet(eq(true));
    }

    @Test
    public void testExpand_WhenSheetStateFull_AndExpandInHalfHeight() {
        when(mBottomSheetController.getSheetState())
                .thenReturn(BottomSheetController.SheetState.FULL);

        mCoordinator.expand(/* expandInHalfHeight= */ true);

        verify(mBottomSheetController).expandSheet(eq(true));
    }
}
