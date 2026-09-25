// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

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
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.IntSupplier;

/** Unit tests for {@link BottomSheetDisclaimerHost}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
public class BottomSheetDisclaimerHostUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private View mContentView;
    @Mock private IntSupplier mVerticalScrollOffsetSupplier;
    @Mock private Consumer<@DismissalCause Integer> mSheetDismissedCallback;

    @Captor private ArgumentCaptor<BottomSheetContent> mSheetContentCaptor;

    private BottomSheetDisclaimerHost mHost;

    public static class NonUserActionReasonsParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(StateChangeReason.NONE).name("None"),
                    new ParameterSet().value(StateChangeReason.NAVIGATION).name("Navigation"),
                    new ParameterSet().value(StateChangeReason.COMPOSITED_UI).name("CompositedUi"),
                    new ParameterSet().value(StateChangeReason.VR).name("Vr"),
                    new ParameterSet().value(StateChangeReason.PROMOTE_TAB).name("PromoteTab"),
                    new ParameterSet().value(StateChangeReason.OMNIBOX_FOCUS).name("OmniboxFocus"));
        }
    }

    @Before
    public void setUp() {
        mHost =
                new BottomSheetDisclaimerHost(
                        mBottomSheetController,
                        mContentView,
                        mVerticalScrollOffsetSupplier,
                        mSheetDismissedCallback);
    }

    /** Shows the host and returns the {@link BottomSheetContent} it requested to show. */
    private BottomSheetContent showAndCaptureSheetContent() {
        mHost.show();
        verify(mBottomSheetController).requestShowContent(mSheetContentCaptor.capture(), eq(true));
        return mSheetContentCaptor.getValue();
    }

    /** Shows the host and makes its content the current sheet content. */
    private BottomSheetContent showAsCurrentSheetContent() {
        BottomSheetContent content = showAndCaptureSheetContent();
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        return content;
    }

    @Test
    public void testConstructor_registersBottomSheetObserver() {
        verify(mBottomSheetController).addObserver(mHost);
    }

    @Test
    public void testShow_requestsShowContent() {
        Assert.assertFalse(mHost.isActive());

        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);
        showAndCaptureSheetContent();

        Assert.assertTrue(mHost.isActive());
    }

    @Test
    public void testSheetContent_wrapsContentView() {
        BottomSheetContent content = showAndCaptureSheetContent();

        Assert.assertSame(mContentView, content.getContentView());
        Assert.assertNull(content.getToolbarView());
    }

    @Test
    public void testSheetContent_verticalScrollOffset_delegatesToSupplier() {
        BottomSheetContent content = showAndCaptureSheetContent();
        when(mVerticalScrollOffsetSupplier.getAsInt()).thenReturn(42);

        Assert.assertEquals(42, content.getVerticalScrollOffset());
    }

    @Test
    public void testSheetContent_configuration() {
        BottomSheetContent content = showAndCaptureSheetContent();

        Assert.assertEquals(BottomSheetContent.ContentPriority.HIGH, content.getPriority());
        Assert.assertTrue(content.swipeToDismissEnabled());
        Assert.assertTrue(content.showHandlebar());
        Assert.assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT, content.getFullHeightRatio(), 0f);
    }

    @Test
    public void testDismiss_hidesContent() {
        BottomSheetContent content = showAndCaptureSheetContent();

        mHost.dismiss(DismissalCause.TAPPED_ACCEPT);

        verify(mBottomSheetController)
                .hideContent(eq(content), eq(true), eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mSheetDismissedCallback, never()).accept(any());
    }

    @Test
    public void testDismiss_invokesCallbackWithDismissalCause() {
        showAsCurrentSheetContent();

        mHost.dismiss(DismissalCause.TAPPED_ACCEPT);
        verify(mSheetDismissedCallback, never()).accept(any());

        mHost.onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);

        verify(mSheetDismissedCallback).accept(DismissalCause.TAPPED_ACCEPT);
    }

    @Test
    public void testDestroy_unregistersObserverAndHidesContent() {
        BottomSheetContent content = showAndCaptureSheetContent();
        Assert.assertTrue(mHost.isActive());

        mHost.destroy();

        Assert.assertFalse(mHost.isActive());
        verify(mBottomSheetController).removeObserver(mHost);
        verify(mBottomSheetController).hideContent(eq(content), eq(false));
        verify(mSheetDismissedCallback, never()).accept(any());
    }

    @Test
    public void testDestroy_calledAfterDismiss_doesNotHideContentAgain() {
        BottomSheetContent content = showAndCaptureSheetContent();
        mHost.dismiss(DismissalCause.TAPPED_ACCEPT);

        mHost.destroy();

        verify(mBottomSheetController, times(1))
                .hideContent(eq(content), eq(true), eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mBottomSheetController, never()).hideContent(eq(content), eq(false));
    }

    @Test
    public void testSheetContentDestroyed_setsInactive() {
        BottomSheetContent content = showAndCaptureSheetContent();
        Assert.assertTrue(mHost.isActive());

        content.destroy();

        Assert.assertFalse(mHost.isActive());
        verify(mSheetDismissedCallback, never()).accept(any());
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
    public void testSheetClosed_swipe_invokesCallbackWithDismissedBySwipeDown() {
        showAsCurrentSheetContent();

        mHost.onSheetOpened(StateChangeReason.NONE);
        mHost.onSheetClosed(StateChangeReason.SWIPE);

        verify(mSheetDismissedCallback).accept(DismissalCause.DISMISSED_BY_SWIPE_DOWN);
    }

    @Test
    public void testSheetClosed_backPress_invokesCallbackWithDismissedByBackPress() {
        showAsCurrentSheetContent();

        mHost.onSheetOpened(StateChangeReason.NONE);
        mHost.onSheetClosed(StateChangeReason.BACK_PRESS);

        verify(mSheetDismissedCallback).accept(DismissalCause.DISMISSED_BY_BACK_PRESS);
    }

    @Test
    public void testSheetClosed_tapScrim_invokesCallbackWithDismissedByTapOutside() {
        showAsCurrentSheetContent();

        mHost.onSheetOpened(StateChangeReason.NONE);
        mHost.onSheetClosed(StateChangeReason.TAP_SCRIM);

        verify(mSheetDismissedCallback).accept(DismissalCause.DISMISSED_BY_TAP_OUTSIDE);
    }

    @Test
    public void testSheetClosed_closeButton_invokesCallbackWithDismissedByCloseButton() {
        showAsCurrentSheetContent();

        mHost.onSheetOpened(StateChangeReason.NONE);
        mHost.onSheetClosed(StateChangeReason.CLOSE_BUTTON);

        verify(mSheetDismissedCallback).accept(DismissalCause.DISMISSED_BY_CLOSE_BUTTON);
    }

    @Test
    @UseMethodParameter(NonUserActionReasonsParams.class)
    public void testSheetClosed_nonUserActionReason_invokesCallbackWithDismissedWithoutUserAction(
            @StateChangeReason int reason) {
        showAsCurrentSheetContent();

        mHost.onSheetOpened(StateChangeReason.NONE);
        mHost.onSheetClosed(reason);

        verify(mSheetDismissedCallback)
                .accept(DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION);
    }
}
