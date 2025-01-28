// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.widget.ViewFlipper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.android.whats_new.WhatsNewProperties.ViewState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Tests {@link WhatsNewCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CLANK_WHATS_NEW})
public class WhatsNewCoordinatorTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetControllerMock;

    private WhatsNewCoordinator mCoordinator;

    @Before
    public void setUp() {
        mCoordinator =
                new WhatsNewCoordinator(
                        ContextUtils.getApplicationContext(), mBottomSheetControllerMock);
    }

    @Test
    public void testShowBottomSheet() {
        mCoordinator.showBottomSheet();

        assertEquals(
                ViewState.OVERVIEW,
                mCoordinator.getModelForTesting().get(WhatsNewProperties.VIEW_STATE));
        verify(mBottomSheetControllerMock).requestShowContent(any(), eq(true));
    }

    @Test
    public void testSetViewState() {
        View contentView = mCoordinator.getView();
        ViewFlipper viewFlipperView =
                (ViewFlipper) contentView.findViewById(R.id.whats_new_page_view_flipper);

        mCoordinator.getModelForTesting().set(WhatsNewProperties.VIEW_STATE, ViewState.OVERVIEW);
        verify(mBottomSheetControllerMock).requestShowContent(any(), eq(true));
        assertEquals(0, viewFlipperView.getDisplayedChild());

        mCoordinator.getModelForTesting().set(WhatsNewProperties.VIEW_STATE, ViewState.DETAIL);
        assertEquals(1, viewFlipperView.getDisplayedChild());

        mCoordinator.getModelForTesting().set(WhatsNewProperties.VIEW_STATE, ViewState.HIDDEN);
        verify(mBottomSheetControllerMock).hideContent(any(), eq(true));
    }
}
