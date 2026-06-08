// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.PopupState;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;

/** Unit tests for {@link LocationBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TOOLBAR_PHONE_ANIMATION_REFACTOR)
public class LocationBarCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UrlBarCoordinator mUrlCoordinator;
    @Mock private View mUrlBar;
    @Mock private LocationBarLayout mLocationBarLayout;
    @Mock private LocationBarEmbedder mLocationBarEmbedder;
    @Mock private View mPlusButton;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private OptionalButtonCoordinator mOptionalButtonCoordinator;
    @Mock private LocationBarMediator mLocationBarMediator;
    @Mock private NewTabPageDelegate mNewTabPageDelegate;

    // LocationBarCoordinator takes a lot of dependencies and a very busy constructor.
    // This allows us to set up tests to verify logic we need to protect without overwhelming test
    // setup. Some tests are better than no tests :P.
    @Mock(answer = Answers.CALLS_REAL_METHODS)
    private LocationBarCoordinator mCoordinator;

    @Before
    public void setUp() {
        mCoordinator.setUrlCoordinatorForTesting(mUrlCoordinator);
        mCoordinator.setUrlBarForTesting(mUrlBar);
        mCoordinator.setLocationBarLayoutForTesting(mLocationBarLayout);
        mCoordinator.setLocationBarEmbedderForTesting(mLocationBarEmbedder);
        mCoordinator.setOptionalButtonCoordinatorForTesting(mOptionalButtonCoordinator);
        mCoordinator.setLocationBarMediatorForTesting(mLocationBarMediator);

        when(mUrlCoordinator.hasFocus()).thenReturn(true);
        when(mLocationBarMediator.getLocationBarDataProvider())
                .thenReturn(mLocationBarDataProvider);
        when(mLocationBarDataProvider.getNewTabPageDelegate()).thenReturn(mNewTabPageDelegate);
        when(mLocationBarLayout.findViewById(R.id.fusebox_plus_button)).thenReturn(mPlusButton);
        when(mLocationBarLayout.getContext()).thenReturn(RuntimeEnvironment.getApplication());
    }

    @Test
    public void testOnFuseboxStateChange_StopsEarlyFromDisabled() {
        mCoordinator.setCurrentFuseboxStateForTesting(FuseboxState.DISABLED);

        mCoordinator.onFuseboxStateChange(FuseboxState.COMPACT);

        // Verify state is updated but animation doesn't run.
        assertEquals(FuseboxState.COMPACT, mCoordinator.getCurrentFuseboxStateForTesting());
        verify(mLocationBarEmbedder, never()).beginEmbeddedDelayedTransition(any(), any());
    }

    @Test
    public void testOnFuseboxStateChange_StopsEarlyToDisabled() {
        mCoordinator.setCurrentFuseboxStateForTesting(FuseboxState.COMPACT);

        mCoordinator.onFuseboxStateChange(FuseboxState.DISABLED);

        // Verify state is updated but animation doesn't run.
        assertEquals(FuseboxState.DISABLED, mCoordinator.getCurrentFuseboxStateForTesting());
        verify(mLocationBarEmbedder, never()).beginEmbeddedDelayedTransition(any(), any());
    }

    @Test
    public void testOnFuseboxStateChange_RunsAnimationForCompactToExpanded() {
        mCoordinator.setCurrentFuseboxStateForTesting(FuseboxState.COMPACT);

        mCoordinator.onFuseboxStateChange(FuseboxState.EXPANDED);

        // Verify state is updated and animation runs by calling beginEmbeddedDelayedTransition.
        assertEquals(FuseboxState.EXPANDED, mCoordinator.getCurrentFuseboxStateForTesting());
        verify(mLocationBarEmbedder).beginEmbeddedDelayedTransition(eq(mLocationBarLayout), any());
    }

    @Test
    public void testInitializeBoundsEllipsis_EnableInTabbedMode() {
        when(mLocationBarDataProvider.getPageClassification(false))
                .thenReturn(PageClassification.OTHER_VALUE);
        mCoordinator.initializeBoundsEllipsis(mLocationBarDataProvider);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(true);
    }

    @Test
    public void testInitializeBoundsEllipsis_DisableInHubSearch() {
        when(mLocationBarDataProvider.getPageClassification(false))
                .thenReturn(PageClassification.ANDROID_HUB_VALUE);
        mCoordinator.initializeBoundsEllipsis(mLocationBarDataProvider);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(false);
    }

    @Test
    public void testInitializeBoundsEllipsis_DisableInCct() {
        when(mLocationBarDataProvider.getPageClassification(false))
                .thenReturn(PageClassification.OTHER_ON_CCT_VALUE);
        mCoordinator.initializeBoundsEllipsis(mLocationBarDataProvider);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(false);
    }

    @Test
    public void testInitializeBoundsEllipsis_DisableInCobrowseComposebox() {
        when(mLocationBarDataProvider.getPageClassification(false))
                .thenReturn(PageClassification.CO_BROWSING_COMPOSEBOX_VALUE);
        mCoordinator.initializeBoundsEllipsis(mLocationBarDataProvider);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(false);
    }

    @Test
    public void testSetMiniOriginMode_Transitions() {
        // Setup default bounds ellipsis
        when(mLocationBarDataProvider.getPageClassification(false))
                .thenReturn(PageClassification.OTHER_VALUE);
        mCoordinator.initializeBoundsEllipsis(mLocationBarDataProvider);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(true);

        mCoordinator.setMiniOriginMode(true);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(false);
        verify(mOptionalButtonCoordinator).hideButton();
        verify(mLocationBarMediator).setMiniOriginMode(true);

        mCoordinator.setMiniOriginMode(false);
        verify(mUrlCoordinator, org.mockito.Mockito.times(2)).setBoundsEllipsisEnabled(true);
        verify(mLocationBarMediator).setMiniOriginMode(false);
    }

    @Test
    public void testOnPopupStateChange_ClearsTextSelectionWhenNotHidden() {
        mCoordinator.onPopupStateChange(PopupState.FLOATING);
        verify(mUrlCoordinator).clearTextSelection();
    }

    @Test
    public void testOnPopupStateChange_DoesNotClearTextSelectionWhenHidden() {
        mCoordinator.onPopupStateChange(PopupState.HIDDEN);
        verify(mUrlCoordinator, never()).clearTextSelection();
    }
}
