// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
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
import org.mockito.Mockito;
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
    @Mock private SideUiContainer mSideUiContainer;
    @Mock private SideUiObserver mSideUiObserver;

    private ViewGroup mStartAnchorContainer;
    private ViewGroup mEndAnchorContainer;
    private View mSideUiContainerView;

    private SideUiCoordinatorImpl mCoordinator;

    @Before
    public void setUp() {
        Context context = Robolectric.buildActivity(Activity.class).setup().get();

        mStartAnchorContainer = new FrameLayout(context);
        mEndAnchorContainer = new FrameLayout(context);
        mSideUiContainerView = new View(context);

        doReturn(mStartAnchorContainer).when(mStartAnchorContainerStub).inflate();
        doReturn(mEndAnchorContainer).when(mEndAnchorContainerStub).inflate();
        doReturn(mSideUiContainerView).when(mSideUiContainer).getView();

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
        // When an observer is added, it's immediately notified of the current specs. This is
        // inferred from the anchor containers' measured width. Re-build with mocks, so we can
        // fake the measure pass and return the expected values.
        ViewStub startAnchorContainerSpyStub = Mockito.mock(ViewStub.class);
        ViewStub endAnchorContainerSpyStub = Mockito.mock(ViewStub.class);

        ViewGroup startAnchorContainerSpy = spy(mStartAnchorContainer);
        ViewGroup endAnchorContainerSpy = spy(mEndAnchorContainer);

        doReturn(startAnchorContainerSpy).when(startAnchorContainerSpyStub).inflate();
        doReturn(endAnchorContainerSpy).when(endAnchorContainerSpyStub).inflate();

        int startX = 25;
        int endX = 75;
        doReturn(startX).when(startAnchorContainerSpy).getMeasuredWidth();
        doReturn(endX).when(endAnchorContainerSpy).getMeasuredWidth();

        mCoordinator =
                new SideUiCoordinatorImpl(startAnchorContainerSpyStub, endAnchorContainerSpyStub);

        // Add the observer after requesting an update.
        mCoordinator.addObserver(mSideUiObserver);

        // Verify the observer is still notified.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(startX, endX);
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
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(width, /* endX= */ 0);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to start container.
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());
        verify(mSideUiContainer).setWidth(width);
    }

    @Test
    public void testRequestUpdateContainer_End() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        int width = 200;
        mCoordinator.requestUpdateContainer(new SideUiContainerProperties(AnchorSide.END, width));

        // Verify observers notified.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(/* startX= */ 0, width);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to end container.
        assertEquals(mEndAnchorContainer, mSideUiContainerView.getParent());
        verify(mSideUiContainer).setWidth(width);
    }

    @Test
    public void testRequestUpdateContainer_DetachOnZeroWidth() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);

        // First attach.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 100));
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());
        clearInvocations(mSideUiContainer);

        // Then update to width 0.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 0));
        assertNull(mSideUiContainerView.getParent());
        verify(mSideUiContainer).setWidth(0);
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
}
