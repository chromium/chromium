// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.ui.base.TestActivity;

import java.util.Collections;
import java.util.Map;

/** Unit tests for {@link SideUiWebContentHairlineManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SideUiWebContentHairlineManagerTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private SideUiStateProvider mSideUiStateProvider;

    private SideUiWebContentHairlineContainer mHairlineContainer;
    private SideUiWebContentHairlineManager mManager;

    private MarginLayoutParams mLayoutParams;

    @Before
    public void setUp() {
        TestActivity activity = Robolectric.buildActivity(TestActivity.class).setup().get();

        mLayoutParams = new MarginLayoutParams(0, 0);

        mHairlineContainer =
                (SideUiWebContentHairlineContainer)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.side_ui_web_content_hairline_container,
                                        /* root= */ null);
        mHairlineContainer.setLayoutParams(mLayoutParams);

        mManager =
                new SideUiWebContentHairlineManager(
                        mBrowserControlsStateProvider, mSideUiStateProvider, mHairlineContainer);
    }

    @Test
    public void testDestroy() {
        ArgumentCaptor<BrowserControlsStateProvider.Observer> controlsObserverCaptor =
                ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsStateProvider).addObserver(controlsObserverCaptor.capture());

        ArgumentCaptor<SideUiObserver> sideUiObserverCaptor =
                ArgumentCaptor.forClass(SideUiObserver.class);
        verify(mSideUiStateProvider).addObserver(sideUiObserverCaptor.capture());

        mManager.destroy();
        verify(mBrowserControlsStateProvider).removeObserver(controlsObserverCaptor.getValue());
        verify(mSideUiStateProvider).removeObserver(sideUiObserverCaptor.getValue());
    }

    @Test
    public void testTopControlsHeightChangedUpdatesMargin() {
        ArgumentCaptor<BrowserControlsStateProvider.Observer> observerCaptor =
                ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsStateProvider).addObserver(observerCaptor.capture());
        BrowserControlsStateProvider.Observer observer = observerCaptor.getValue();
        View topHairline = mHairlineContainer.getTopHairline();

        // 1. Non-zero offset: show top hairline.
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(100f);
        observer.onTopControlsHeightChanged(100, 0);

        assertEquals("Top margin should be updated.", 100, mLayoutParams.topMargin);
        assertEquals(View.VISIBLE, topHairline.getVisibility());

        // 2. Zero offset: hide top hairline.
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(0f);
        observer.onTopControlsHeightChanged(0, 0);

        assertEquals("Top margin should be updated to 0.", 0, mLayoutParams.topMargin);
        assertEquals(View.INVISIBLE, topHairline.getVisibility());
    }

    @Test
    public void testControlsOffsetChangedUpdatesMargin() {
        ArgumentCaptor<BrowserControlsStateProvider.Observer> observerCaptor =
                ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsStateProvider).addObserver(observerCaptor.capture());
        BrowserControlsStateProvider.Observer observer = observerCaptor.getValue();
        View topHairline = mHairlineContainer.getTopHairline();

        // 1. Non-zero offset: show top hairline.
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(50f);
        observer.onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);

        assertEquals("Top margin should be updated.", 50, mLayoutParams.topMargin);
        assertEquals(View.VISIBLE, topHairline.getVisibility());

        // 2. Zero offset: hide top hairline.
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(0f);
        observer.onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);

        assertEquals("Top margin should be updated to 0.", 0, mLayoutParams.topMargin);
        assertEquals(View.INVISIBLE, topHairline.getVisibility());
    }

    @Test
    public void testHairlineVisibilityChangesDuringTransitions() {
        ArgumentCaptor<SideUiObserver> observerCaptor =
                ArgumentCaptor.forClass(SideUiObserver.class);
        verify(mSideUiStateProvider).addObserver(observerCaptor.capture());
        SideUiObserver observer = observerCaptor.getValue();
        View leftHairline = mHairlineContainer.getLeftHairline();
        View rightHairline = mHairlineContainer.getRightHairline();

        // 1. Assert initially INVISIBLE.
        assertEquals(View.INVISIBLE, leftHairline.getVisibility());
        assertEquals(View.INVISIBLE, rightHairline.getVisibility());

        // 2. Show left SideUI.
        SideUiSpecs showLeftSpecs = new SideUiSpecs(Map.of(AnchorSide.LEFT, 100));
        observer.onSideUiSpecsChanged(showLeftSpecs);
        assertEquals(View.VISIBLE, leftHairline.getVisibility());
        assertEquals(View.INVISIBLE, rightHairline.getVisibility());

        // 3. Hide left SideUI and show right SideUI.
        SideUiSpecs showRightSpecs = new SideUiSpecs(Map.of(AnchorSide.RIGHT, 50));
        observer.onSideUiSpecsChanged(showRightSpecs);
        assertEquals(View.INVISIBLE, leftHairline.getVisibility());
        assertEquals(View.VISIBLE, rightHairline.getVisibility());

        // 4. Hide right SideUI.
        SideUiSpecs hideAllSpecs = new SideUiSpecs(Collections.emptyMap());
        observer.onSideUiSpecsChanged(hideAllSpecs);
        assertEquals(View.INVISIBLE, leftHairline.getVisibility());
        assertEquals(View.INVISIBLE, rightHairline.getVisibility());
    }
}
