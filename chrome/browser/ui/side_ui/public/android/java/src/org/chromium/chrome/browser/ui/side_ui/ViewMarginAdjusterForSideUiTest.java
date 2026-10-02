// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;

import android.util.ArrayMap;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;

import java.util.Map;

/** Tests for {@link ViewMarginAdjusterForSideUi}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ViewMarginAdjusterForSideUiTest {
    private View mView;
    private MarginLayoutParams mMarginLayoutParams;

    @Before
    public void setUp() {
        mView = new View(ContextUtils.getApplicationContext());
        mMarginLayoutParams = new MarginLayoutParams(0, 0);
        mView.setLayoutParams(mMarginLayoutParams);
    }

    @Test
    public void testOnSideUiSpecsChanged_noBaseMargin() {
        SideUiObserver marginContainerObserver = new ViewMarginAdjusterForSideUi(mView);

        // End margin
        marginContainerObserver.onSideUiSpecsChanged(
                new SideUiSpecs(0, 200),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        MarginLayoutParams params = (MarginLayoutParams) mView.getLayoutParams();
        assertEquals(0, params.getMarginStart());
        assertEquals(200, params.getMarginEnd());

        // Start margin
        marginContainerObserver.onSideUiSpecsChanged(
                new SideUiSpecs(200, 0),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        params = (MarginLayoutParams) mView.getLayoutParams();
        assertEquals(200, params.getMarginStart());
        assertEquals(0, params.getMarginEnd());

        // Both margins
        marginContainerObserver.onSideUiSpecsChanged(
                new SideUiSpecs(100, 200),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        params = (MarginLayoutParams) mView.getLayoutParams();
        assertEquals(100, params.getMarginStart());
        assertEquals(200, params.getMarginEnd());
    }

    @Test
    public void testOnSideUiSpecsChanged_hasBaseMargin() {
        mMarginLayoutParams.leftMargin = 20;
        mMarginLayoutParams.rightMargin = 35;

        SideUiObserver marginContainerObserver = new ViewMarginAdjusterForSideUi(mView);

        // Right margin
        marginContainerObserver.onSideUiSpecsChanged(
                new SideUiSpecs(0, 200),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        MarginLayoutParams params = (MarginLayoutParams) mView.getLayoutParams();
        assertEquals(20, params.leftMargin);
        assertEquals(235, params.rightMargin);

        // Start margin
        marginContainerObserver.onSideUiSpecsChanged(
                new SideUiSpecs(200, 0),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        params = (MarginLayoutParams) mView.getLayoutParams();
        assertEquals(220, params.leftMargin);
        assertEquals(35, params.rightMargin);

        // Both margins
        marginContainerObserver.onSideUiSpecsChanged(
                new SideUiSpecs(100, 200),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        params = (MarginLayoutParams) mView.getLayoutParams();
        assertEquals(120, params.leftMargin);
        assertEquals(235, params.rightMargin);
    }

    @Test
    public void testOnSideUiSpecsChanged_toolbarElementIgnoresWebContentsHeightType() {
        SideUiObserver marginContainerObserver =
                new ViewMarginAdjusterForSideUi(mView, /* forToolbarElement= */ true);

        Map<@AnchorSide Integer, SideUiSize> sideUiSpecs = new ArrayMap<>();
        sideUiSpecs.put(AnchorSide.LEFT, new SideUiSize(100, HeightType.WEB_CONTENTS));
        sideUiSpecs.put(AnchorSide.RIGHT, new SideUiSize(200, HeightType.TOOLBAR));
        marginContainerObserver.onSideUiSpecsChanged(
                new SideUiSpecs(sideUiSpecs),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        MarginLayoutParams params = (MarginLayoutParams) mView.getLayoutParams();

        // Ignores the width from WEB_CONTENTS-heighType container.
        assertEquals(0, params.getMarginStart());

        // Respects the width from TOOLBAR-heighType container.
        assertEquals(200, params.getMarginEnd());
    }
}
