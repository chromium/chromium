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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.ui.base.TestActivity;

import java.util.Collections;
import java.util.Map;

/** Unit tests for {@link SideUiWebContentHairlineManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.SIDE_PANEL_TOP_HAIRLINE_REFACTOR_ANDROID)
public class SideUiWebContentHairlineManagerTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private SideUiStateProvider mSideUiStateProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;

    private SideUiWebContentHairlineManager mManager;

    private View mTopHairline;
    private View mLeftHairline;
    private View mTopLeftRoundedCorner;
    private View mRightHairline;
    private View mTopRightRoundedCorner;
    private View mBottomLeftRoundedCorner;

    private MarginLayoutParams mLayoutParams;

    @Before
    public void setUp() {
        TestActivity activity = Robolectric.buildActivity(TestActivity.class).setup().get();

        mLayoutParams = new MarginLayoutParams(0, 0);

        SideUiWebContentHairlineContainer hairlineContainer =
                (SideUiWebContentHairlineContainer)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.side_ui_web_content_hairline_container,
                                        /* root= */ null);
        hairlineContainer.setLayoutParams(mLayoutParams);

        mTopHairline = hairlineContainer.getTopHairline();
        mLeftHairline = hairlineContainer.getLeftHairline();
        mTopLeftRoundedCorner = hairlineContainer.getTopLeftRoundedCorner();
        mRightHairline = hairlineContainer.getRightHairline();
        mTopRightRoundedCorner = hairlineContainer.getTopRightRoundedCorner();
        mBottomLeftRoundedCorner = hairlineContainer.getBottomLeftRoundedCorner();

        mManager =
                new SideUiWebContentHairlineManager(
                        mBrowserControlsStateProvider,
                        mSideUiStateProvider,
                        hairlineContainer,
                        mIncognitoStateProvider);
    }

    @Test
    public void testDestroy() {
        ArgumentCaptor<BrowserControlsStateProvider.Observer> controlsObserverCaptor =
                ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsStateProvider).addObserver(controlsObserverCaptor.capture());

        ArgumentCaptor<SideUiObserver> sideUiObserverCaptor =
                ArgumentCaptor.forClass(SideUiObserver.class);
        verify(mSideUiStateProvider).addObserver(sideUiObserverCaptor.capture());

        ArgumentCaptor<IncognitoStateProvider.IncognitoStateObserver> incognitoObserverCaptor =
                ArgumentCaptor.forClass(IncognitoStateProvider.IncognitoStateObserver.class);
        verify(mIncognitoStateProvider)
                .addIncognitoStateObserverAndTrigger(incognitoObserverCaptor.capture());

        mManager.destroy();
        verify(mBrowserControlsStateProvider).removeObserver(controlsObserverCaptor.getValue());
        verify(mSideUiStateProvider).removeObserver(sideUiObserverCaptor.getValue());
        verify(mIncognitoStateProvider).removeObserver(incognitoObserverCaptor.getValue());
    }

    @Test
    public void testTopControlsHeightChangedUpdatesMargin() {
        ArgumentCaptor<BrowserControlsStateProvider.Observer> observerCaptor =
                ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsStateProvider).addObserver(observerCaptor.capture());
        BrowserControlsStateProvider.Observer observer = observerCaptor.getValue();

        // 1. Non-zero offset and side UI showing: show top hairline.
        when(mSideUiStateProvider.isAnySideUiShowing()).thenReturn(true);
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(100f);
        observer.onTopControlsHeightChanged(100, 0);

        assertEquals("Top margin should be updated.", 100, mLayoutParams.topMargin);
        assertEquals(View.VISIBLE, mTopHairline.getVisibility());

        // 2. Zero offset: hide top hairline.
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(0f);
        observer.onTopControlsHeightChanged(0, 0);

        assertEquals("Top margin should be updated to 0.", 0, mLayoutParams.topMargin);
        assertEquals(View.INVISIBLE, mTopHairline.getVisibility());
    }

    @Test
    public void testControlsOffsetChangedUpdatesMargin() {
        ArgumentCaptor<BrowserControlsStateProvider.Observer> observerCaptor =
                ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsStateProvider).addObserver(observerCaptor.capture());
        BrowserControlsStateProvider.Observer observer = observerCaptor.getValue();

        // 1. Non-zero offset and side UI showing: show top hairline.
        when(mSideUiStateProvider.isAnySideUiShowing()).thenReturn(true);
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(50f);
        observer.onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);

        assertEquals("Top margin should be updated.", 50, mLayoutParams.topMargin);
        assertEquals(View.VISIBLE, mTopHairline.getVisibility());

        // 2. Zero offset: hide top hairline.
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(0f);
        observer.onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);

        assertEquals("Top margin should be updated to 0.", 0, mLayoutParams.topMargin);
        assertEquals(View.INVISIBLE, mTopHairline.getVisibility());
    }

    @Test
    public void testUpdate() {
        when(mSideUiStateProvider.isAnySideUiShowing()).thenReturn(true);
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(100f);
        mManager.update();

        assertEquals("Top margin should be updated.", 100, mLayoutParams.topMargin);
        assertEquals(View.VISIBLE, mTopHairline.getVisibility());

        when(mSideUiStateProvider.isAnySideUiShowing()).thenReturn(false);
        mManager.update();
        assertEquals(View.INVISIBLE, mTopHairline.getVisibility());
    }

    @Test
    public void testHairlineVisibilityChangesDuringTransitions() {
        ArgumentCaptor<SideUiObserver> observerCaptor =
                ArgumentCaptor.forClass(SideUiObserver.class);
        verify(mSideUiStateProvider).addObserver(observerCaptor.capture());
        SideUiObserver observer = observerCaptor.getValue();

        // 1. Assert initially INVISIBLE.
        assertEquals(View.INVISIBLE, mLeftHairline.getVisibility());
        assertEquals(View.INVISIBLE, mTopLeftRoundedCorner.getVisibility());
        assertEquals(View.INVISIBLE, mRightHairline.getVisibility());
        assertEquals(View.INVISIBLE, mTopRightRoundedCorner.getVisibility());
        assertEquals(View.INVISIBLE, mBottomLeftRoundedCorner.getVisibility());

        // 2. Show left SideUI.
        SideUiSpecs showLeftSpecs =
                new SideUiSpecs(Map.of(AnchorSide.LEFT, new SideUiSize(100, HeightType.TOOLBAR)));
        observer.onSideUiSpecsChanged(showLeftSpecs);
        assertEquals(View.VISIBLE, mLeftHairline.getVisibility());
        assertEquals(View.VISIBLE, mTopLeftRoundedCorner.getVisibility());
        assertEquals(View.INVISIBLE, mRightHairline.getVisibility());
        assertEquals(View.INVISIBLE, mTopRightRoundedCorner.getVisibility());
        assertEquals(View.INVISIBLE, mBottomLeftRoundedCorner.getVisibility());

        // 3. Hide left SideUI and show right SideUI.
        SideUiSpecs showRightSpecs =
                new SideUiSpecs(Map.of(AnchorSide.RIGHT, new SideUiSize(50, HeightType.TOOLBAR)));
        observer.onSideUiSpecsChanged(showRightSpecs);
        assertEquals(View.INVISIBLE, mLeftHairline.getVisibility());
        assertEquals(View.INVISIBLE, mTopLeftRoundedCorner.getVisibility());
        assertEquals(View.VISIBLE, mRightHairline.getVisibility());
        assertEquals(View.VISIBLE, mTopRightRoundedCorner.getVisibility());
        assertEquals(View.INVISIBLE, mBottomLeftRoundedCorner.getVisibility());

        // 4. Hide right SideUI.
        SideUiSpecs hideAllSpecs = new SideUiSpecs(Collections.emptyMap());
        observer.onSideUiSpecsChanged(hideAllSpecs);
        assertEquals(View.INVISIBLE, mLeftHairline.getVisibility());
        assertEquals(View.INVISIBLE, mTopLeftRoundedCorner.getVisibility());
        assertEquals(View.INVISIBLE, mRightHairline.getVisibility());
        assertEquals(View.INVISIBLE, mTopRightRoundedCorner.getVisibility());
        assertEquals(View.INVISIBLE, mBottomLeftRoundedCorner.getVisibility());
    }

    @Test
    public void testHairlineVisibilityForVerticalTabs() {
        ArgumentCaptor<SideUiObserver> observerCaptor =
                ArgumentCaptor.forClass(SideUiObserver.class);
        verify(mSideUiStateProvider).addObserver(observerCaptor.capture());
        SideUiObserver observer = observerCaptor.getValue();

        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(100f);
        when(mSideUiStateProvider.isSideUiShowing(SideUiId.VERTICAL_TABS)).thenReturn(true);

        SideUiSpecs showLeftSpecs =
                new SideUiSpecs(Map.of(AnchorSide.LEFT, new SideUiSize(100, HeightType.TOOLBAR)));
        observer.onSideUiSpecsChanged(showLeftSpecs);

        assertEquals(View.INVISIBLE, mLeftHairline.getVisibility());
        assertEquals(View.INVISIBLE, mTopLeftRoundedCorner.getVisibility());
        assertEquals(View.INVISIBLE, mRightHairline.getVisibility());
        assertEquals(View.INVISIBLE, mTopRightRoundedCorner.getVisibility());
        assertEquals(View.VISIBLE, mBottomLeftRoundedCorner.getVisibility());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SIDE_PANEL_TOP_HAIRLINE_REFACTOR_ANDROID)
    public void testTopHairlineHiddenWhenFeatureDisabled() {
        ArgumentCaptor<BrowserControlsStateProvider.Observer> observerCaptor =
                ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsStateProvider).addObserver(observerCaptor.capture());
        BrowserControlsStateProvider.Observer observer = observerCaptor.getValue();

        // Even with non-zero offset and side UI showing, if the feature is disabled,
        // the top hairline must remain INVISIBLE.
        when(mSideUiStateProvider.isAnySideUiShowing()).thenReturn(true);
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(100f);
        observer.onTopControlsHeightChanged(100, 0);

        assertEquals("Top margin should still be updated.", 100, mLayoutParams.topMargin);
        assertEquals(View.INVISIBLE, mTopHairline.getVisibility());
    }
}
