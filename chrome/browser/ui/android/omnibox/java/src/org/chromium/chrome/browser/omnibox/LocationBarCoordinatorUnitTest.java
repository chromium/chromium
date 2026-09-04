// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
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
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.PopupState;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;

/** Unit tests for {@link LocationBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TOOLBAR_PHONE_ANIMATION_REFACTOR)
public class LocationBarCoordinatorUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private UrlBarCoordinator mUrlCoordinator;
    @Mock private FuseboxCoordinator mFuseboxCoordinator;
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

    private final SettableNonNullObservableSupplier<Integer> mFuseboxLayoutModeSupplier =
            ObservableSuppliers.createNonNull(FuseboxLayoutMode.TOOLBAR);

    @Before
    public void setUp() {
        mCoordinator.setUrlCoordinatorForTesting(mUrlCoordinator);
        mCoordinator.setFuseboxCoordinatorForTesting(mFuseboxCoordinator);
        mCoordinator.setUrlBarForTesting(mUrlBar);
        mCoordinator.setLocationBarLayoutForTesting(mLocationBarLayout);
        mCoordinator.setLocationBarEmbedderForTesting(mLocationBarEmbedder);
        mCoordinator.setOptionalButtonCoordinatorForTesting(mOptionalButtonCoordinator);
        mCoordinator.setLocationBarMediatorForTesting(mLocationBarMediator);

        lenient().when(mUrlCoordinator.hasFocus()).thenReturn(true);
        lenient()
                .when(mLocationBarMediator.getLocationBarDataProvider())
                .thenReturn(mLocationBarDataProvider);
        lenient()
                .when(mLocationBarDataProvider.getNewTabPageDelegate())
                .thenReturn(mNewTabPageDelegate);
        lenient()
                .when(mLocationBarLayout.findViewById(R.id.fusebox_plus_button))
                .thenReturn(mPlusButton);
        lenient()
                .when(mLocationBarLayout.getContext())
                .thenReturn(RuntimeEnvironment.getApplication());
        lenient()
                .when(mFuseboxCoordinator.getFuseboxLayoutModeSupplier())
                .thenReturn(mFuseboxLayoutModeSupplier);
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
    public void testOnFuseboxStateChange_StopsEarlyFromPopover() {
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        when(mFuseboxCoordinator.getFuseboxLayoutModeSupplier())
                .thenReturn(mFuseboxLayoutModeSupplier);
        mCoordinator.setCurrentFuseboxStateForTesting(FuseboxState.COMPACT);

        mCoordinator.onFuseboxStateChange(FuseboxState.EXPANDED);

        // Verify state is updated but animation doesn't run.
        assertEquals(FuseboxState.EXPANDED, mCoordinator.getCurrentFuseboxStateForTesting());
        verify(mLocationBarEmbedder, never()).beginEmbeddedDelayedTransition(any(), any());
    }

    @Test
    public void testInitializeBoundsEllipsis_EnableInTabbedMode() {
        when(mLocationBarDataProvider.getPageClassification(/* prefetch= */ false))
                .thenReturn(PageClassification.OTHER);
        mCoordinator.initializeBoundsEllipsis(mLocationBarDataProvider);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(true);
    }

    @Test
    public void testInitializeBoundsEllipsis_DisableInHubSearch() {
        when(mLocationBarDataProvider.getPageClassification(/* prefetch= */ false))
                .thenReturn(PageClassification.ANDROID_HUB);
        mCoordinator.initializeBoundsEllipsis(mLocationBarDataProvider);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(false);
    }

    @Test
    public void testInitializeBoundsEllipsis_DisableInCct() {
        when(mLocationBarDataProvider.getPageClassification(/* prefetch= */ false))
                .thenReturn(PageClassification.OTHER_ON_CCT);
        mCoordinator.initializeBoundsEllipsis(mLocationBarDataProvider);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(false);
    }

    @Test
    public void testSetMiniOriginMode_Transitions() {
        // Setup default bounds ellipsis
        when(mLocationBarDataProvider.getPageClassification(/* prefetch= */ false))
                .thenReturn(PageClassification.OTHER);
        mCoordinator.initializeBoundsEllipsis(mLocationBarDataProvider);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(true);

        mCoordinator.setMiniOriginMode(true);
        verify(mUrlCoordinator).setBoundsEllipsisEnabled(false);
        verify(mOptionalButtonCoordinator).hideButton();
        verify(mLocationBarMediator).setMiniOriginMode(true);

        mCoordinator.setMiniOriginMode(false);
        verify(mUrlCoordinator, times(2)).setBoundsEllipsisEnabled(true);
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

    @Test
    public void testOnTextWrappingChanged() {
        mCoordinator.onTextWrappingChanged(/* isWrapping= */ true);
        verify(mFuseboxCoordinator).onFuseboxTextWrappingChanged(/* isTextWrapping= */ true);
        verify(mLocationBarMediator).setIsTextWrapping(true);
        verify(mLocationBarMediator).updateButtonVisibility();
    }
}
