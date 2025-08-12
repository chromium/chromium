// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.incognito;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;

/** Unit tests for {@link IncognitoIndicatorCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
public class IncognitoIndicatorCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ToolbarLayout mParentToolbar;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private ViewStub mIncognitoIndicatorStub;
    @Mock private View mIncognitoIndicatorView;

    private IncognitoIndicatorCoordinator mCoordinator;

    @Before
    public void setUp() {
        when(mParentToolbar.findViewById(eq(R.id.incognito_indicator_stub)))
                .thenReturn(mIncognitoIndicatorStub);
        when(mIncognitoIndicatorStub.inflate()).thenReturn(mIncognitoIndicatorView);

        mCoordinator =
                new IncognitoIndicatorCoordinator(
                        mParentToolbar,
                        mThemeColorProvider,
                        mIncognitoStateProvider,
                        /* visible= */ false);
        assertNull(
                "Indicator should not be inflated initially.",
                mCoordinator.getIncognitoIndicatorViewForTesting());
    }

    @Test
    public void testOnIncognitoStateChanged_TogglesVisibility() {
        mCoordinator.setVisibility(/* visible= */ true);

        // Start not in incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ false);
        assertNull(
                "Indicator should not be inflated when not in incognito.",
                mCoordinator.getIncognitoIndicatorViewForTesting());

        // Transition to incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);
        assertNotNull(
                "Indicator should be inflated.",
                mCoordinator.getIncognitoIndicatorViewForTesting());
        verify(mIncognitoIndicatorView).setVisibility(View.VISIBLE);
        clearInvocations(mIncognitoIndicatorView);

        // Transition back out of incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ false);
        verify(mIncognitoIndicatorView).setVisibility(View.GONE);
    }

    @Test
    public void testUpdateVisibility_TogglesVisibility() {
        // Start in incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);
        assertNotNull(
                "Indicator should be inflated.",
                mCoordinator.getIncognitoIndicatorViewForTesting());
        verify(mIncognitoIndicatorView).setVisibility(View.GONE);
        clearInvocations(mIncognitoIndicatorView);

        // Show toolbar buttons.
        mCoordinator.setVisibility(/* visible= */ true);
        assertNotNull(
                "Indicator should be inflated.",
                mCoordinator.getIncognitoIndicatorViewForTesting());
        verify(mIncognitoIndicatorView).setVisibility(View.VISIBLE);
        clearInvocations(mIncognitoIndicatorView);

        // Hide toolbar buttons.
        mCoordinator.setVisibility(/* visible= */ false);
        verify(mIncognitoIndicatorView).setVisibility(View.GONE);
        clearInvocations(mIncognitoIndicatorView);
    }
}
