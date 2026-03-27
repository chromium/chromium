// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;

/** Unit tests for {@link LocationBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TOOLBAR_PHONE_ANIMATION_REFACTOR)
public class LocationBarCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UrlBarCoordinator mUrlCoordinator;
    @Mock private LocationBarLayout mLocationBarLayout;
    @Mock private LocationBarEmbedder mLocationBarEmbedder;
    @Mock private View mAddButton;

    // LocationBarCoordinator takes a lot of dependencies and a very busy constructor.
    // This allows us to set up tests to verify logic we need to protect without overwhelming test
    // setup. Some tests are better than no tests :P.
    @Mock(answer = Answers.CALLS_REAL_METHODS)
    private LocationBarCoordinator mCoordinator;

    @Before
    public void setUp() {
        mCoordinator.setUrlCoordinatorForTesting(mUrlCoordinator);
        mCoordinator.setLocationBarLayoutForTesting(mLocationBarLayout);
        mCoordinator.setLocationBarEmbedderForTesting(mLocationBarEmbedder);

        when(mUrlCoordinator.hasFocus()).thenReturn(true);
        when(mLocationBarLayout.findViewById(R.id.location_bar_attachments_add))
                .thenReturn(mAddButton);
        when(mLocationBarLayout.getContext())
                .thenReturn(org.robolectric.RuntimeEnvironment.getApplication());
    }

    @Test
    public void testOnFuseboxStateChange_StopsEarlyFromDisabled() {
        mCoordinator.setCurrentFuseboxStateForTesting(FuseboxState.DISABLED);

        mCoordinator.onFuseboxStateChange(FuseboxState.COMPACT);

        // Verify state is updated but animation doesn't run.
        Assert.assertEquals(FuseboxState.COMPACT, mCoordinator.getCurrentFuseboxStateForTesting());
        verify(mLocationBarEmbedder, never()).beginEmbeddedDelayedTransition(any(), any());
    }

    @Test
    public void testOnFuseboxStateChange_StopsEarlyToDisabled() {
        mCoordinator.setCurrentFuseboxStateForTesting(FuseboxState.COMPACT);

        mCoordinator.onFuseboxStateChange(FuseboxState.DISABLED);

        // Verify state is updated but animation doesn't run.
        Assert.assertEquals(FuseboxState.DISABLED, mCoordinator.getCurrentFuseboxStateForTesting());
        verify(mLocationBarEmbedder, never()).beginEmbeddedDelayedTransition(any(), any());
    }

    @Test
    public void testOnFuseboxStateChange_RunsAnimationForCompactToExpanded() {
        mCoordinator.setCurrentFuseboxStateForTesting(FuseboxState.COMPACT);

        mCoordinator.onFuseboxStateChange(FuseboxState.EXPANDED);

        // Verify state is updated and animation runs by calling beginEmbeddedDelayedTransition.
        Assert.assertEquals(FuseboxState.EXPANDED, mCoordinator.getCurrentFuseboxStateForTesting());
        verify(mLocationBarEmbedder).beginEmbeddedDelayedTransition(eq(mLocationBarLayout), any());
    }
}
