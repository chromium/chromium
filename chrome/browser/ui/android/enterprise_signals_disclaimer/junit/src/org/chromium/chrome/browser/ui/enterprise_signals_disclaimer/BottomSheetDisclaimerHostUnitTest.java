// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.params.BlockJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

/** Unit tests for {@link BottomSheetDisclaimerHost}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
public class BottomSheetDisclaimerHostUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private EnterpriseSignalsDisclaimerBottomSheetView mSheetContent;
    @Mock private Consumer<Boolean> mSheetDismissedCallback;

    @Captor private ArgumentCaptor<Runnable> mDestroyedCallbackCaptor;

    private BottomSheetDisclaimerHost mHost;

    public static class UserActionReasonsParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(StateChangeReason.SWIPE).name("Swipe"),
                    new ParameterSet().value(StateChangeReason.BACK_PRESS).name("BackPress"),
                    new ParameterSet().value(StateChangeReason.TAP_SCRIM).name("TapScrim"),
                    new ParameterSet().value(StateChangeReason.CLOSE_BUTTON).name("CloseButton"));
        }
    }

    public static class NonUserActionReasonsParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(StateChangeReason.NONE).name("None"),
                    new ParameterSet().value(StateChangeReason.NAVIGATION).name("Navigation"),
                    new ParameterSet().value(StateChangeReason.COMPOSITED_UI).name("CompositedUi"),
                    new ParameterSet().value(StateChangeReason.VR).name("Vr"),
                    new ParameterSet().value(StateChangeReason.PROMOTE_TAB).name("PromoteTab"),
                    new ParameterSet().value(StateChangeReason.OMNIBOX_FOCUS).name("OmniboxFocus"),
                    new ParameterSet()
                            .value(StateChangeReason.INTERACTION_COMPLETE)
                            .name("InteractionComplete"));
        }
    }

    @Before
    public void setUp() {
        mHost =
                new BottomSheetDisclaimerHost(
                        mBottomSheetController, mSheetContent, mSheetDismissedCallback);
    }

    @Test
    public void testConstructor_registersBottomSheetObserver() {
        verify(mBottomSheetController).addObserver(mHost);
    }

    @Test
    public void testShow_requestsShowContent() {
        Assert.assertFalse(mHost.isActive());

        when(mBottomSheetController.requestShowContent(eq(mSheetContent), eq(true)))
                .thenReturn(true);
        mHost.show();

        Assert.assertTrue(mHost.isActive());
        verify(mSheetContent).setOnDestroyedCallback(any());
        verify(mBottomSheetController).requestShowContent(eq(mSheetContent), eq(true));
    }

    @Test
    public void testHide_hidesContent() {
        mHost.show();

        mHost.hide();

        verify(mBottomSheetController).hideContent(eq(mSheetContent), eq(true));
    }

    @Test
    public void testDestroy_unregistersObserverAndHidesContent() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.destroy();

        Assert.assertFalse(mHost.isActive());
        verify(mBottomSheetController).removeObserver(mHost);
        verify(mBottomSheetController).hideContent(eq(mSheetContent), eq(false));
    }

    @Test
    public void testOnDestroyedCallback_setsInactive() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        verify(mSheetContent).setOnDestroyedCallback(mDestroyedCallbackCaptor.capture());
        Runnable callback = mDestroyedCallbackCaptor.getValue();
        Assert.assertNotNull(callback);

        callback.run();

        Assert.assertFalse(mHost.isActive());
    }

    @Test
    public void testOtherSheetOpens() {
        BottomSheetContent otherContent = mock(BottomSheetContent.class);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(otherContent);

        mHost.onSheetOpened(StateChangeReason.NONE);
        mHost.onSheetClosed(StateChangeReason.SWIPE);

        verify(mSheetDismissedCallback, never()).accept(any());
    }

    @Test
    @UseMethodParameter(UserActionReasonsParams.class)
    public void testSheetClosed_userActionReason_invokesCallbackWithTrue(
            @StateChangeReason int reason) {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);

        mHost.show();
        mHost.onSheetOpened(StateChangeReason.NONE);
        mHost.onSheetClosed(reason);

        verify(mSheetDismissedCallback).accept(true);
    }

    @Test
    @UseMethodParameter(NonUserActionReasonsParams.class)
    public void testSheetClosed_nonUserActionReason_invokesCallbackWithFalse(
            @StateChangeReason int reason) {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);

        mHost.show();
        mHost.onSheetOpened(StateChangeReason.NONE);
        mHost.onSheetClosed(reason);

        verify(mSheetDismissedCallback).accept(false);
    }
}
