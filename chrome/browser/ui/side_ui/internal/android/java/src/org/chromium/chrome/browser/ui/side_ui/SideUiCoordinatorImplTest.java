// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiContainerProperties;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

/** Unit tests for {@link SideUiCoordinatorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SideUiCoordinatorImplTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ViewStub mStartAnchorContainerStub;
    @Mock private ViewStub mEndAnchorContainerStub;
    @Mock private SideUiObserver mSideUiObserver;

    private ViewGroup mStartAnchorContainer;
    private ViewGroup mEndAnchorContainer;
    private View mSideUiContainerView;
    private TestSideUiContainer mSideUiContainer;

    private SideUiCoordinatorImpl mCoordinator;

    @Before
    public void setUp() {
        Context context = Robolectric.buildActivity(Activity.class).setup().get();

        mStartAnchorContainer = new FrameLayout(context);
        mEndAnchorContainer = new FrameLayout(context);
        mSideUiContainerView = new View(context);
        mSideUiContainer = new TestSideUiContainer(mSideUiContainerView);

        doReturn(mStartAnchorContainer).when(mStartAnchorContainerStub).inflate();
        doReturn(mEndAnchorContainer).when(mEndAnchorContainerStub).inflate();

        mCoordinator =
                new SideUiCoordinatorImpl(mStartAnchorContainerStub, mEndAnchorContainerStub);
    }

    @Test
    public void testRegisterSideUiContainer() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        assertEquals(
                "Unexpected registered SideUiContainer.",
                mCoordinator.getSideUiContainerForTesting(),
                mSideUiContainer);

        mCoordinator.unregisterSideUiContainer(mSideUiContainer);
        assertNull(
                "Registered SideUiContainer expected to be null.",
                mCoordinator.getSideUiContainerForTesting());
    }

    @Test
    public void testAddObserver_NotifyCurrentSpecs() {
        int startContainerWidth = 25;
        int endContainerWidth = 75;
        mStartAnchorContainer.setMinimumWidth(startContainerWidth);
        mEndAnchorContainer.setMinimumWidth(endContainerWidth);

        // Add the observer after requesting an update.
        mCoordinator.addObserver(mSideUiObserver);

        // Verify the observer is still notified.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(startContainerWidth, endContainerWidth);
        verify(mSideUiObserver).onSideUiSpecsChanged(expectedSideUiSpecs);
    }

    @Test
    public void testRemoveObserver_NotifyEmptySpecs() {
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        // Verify the observer is notified with empty specs on removal.
        mCoordinator.removeObserver(mSideUiObserver);
        verify(mSideUiObserver).onSideUiSpecsChanged(SideUiSpecs.EMPTY_SIDE_UI_SPECS);
    }

    @Test
    public void testRequestUpdateContainer_Start() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        int width = 100;
        mCoordinator.requestUpdateContainer(new SideUiContainerProperties(AnchorSide.START, width));

        // Verify observers notified.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(width, /* endContainerWidth= */ 0);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to start container.
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());
        assertEquals(width, getSideUiContainerViewWidth());
    }

    @Test
    public void testRequestUpdateContainer_End() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        int width = 200;
        mCoordinator.requestUpdateContainer(new SideUiContainerProperties(AnchorSide.END, width));

        // Verify observers notified.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(/* startContainerWidth= */ 0, width);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to end container.
        assertEquals(mEndAnchorContainer, mSideUiContainerView.getParent());
        assertEquals(width, getSideUiContainerViewWidth());
    }

    @Test
    public void testRequestUpdateContainer_DetachOnZeroWidth() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);

        // First attach.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 100));
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());

        // Then update to width 0.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 0));
        assertNull(mSideUiContainerView.getParent());
        assertEquals(0, getSideUiContainerViewWidth());
    }

    @Test
    public void testSwitchAnchorSides() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);

        // Start at START.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 100));
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());

        // Switch to END.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, /* width= */ 200));
        assertEquals(mEndAnchorContainer, mSideUiContainerView.getParent());
    }

    private int getSideUiContainerViewWidth() {
        return mSideUiContainerView.getLayoutParams().width;
    }
}
