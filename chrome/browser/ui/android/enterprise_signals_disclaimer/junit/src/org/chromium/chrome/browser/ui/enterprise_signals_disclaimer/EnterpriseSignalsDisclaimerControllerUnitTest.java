// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;

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
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController.CoordinatorFactory;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController.HostFactory;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController.ShownOn;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerCoordinator.PresentationMode;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.function.Consumer;

/** Unit tests for {@link EnterpriseSignalsDisclaimerController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class EnterpriseSignalsDisclaimerControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IdentityManager mIdentityManager;
    @Mock private EnterpriseSignalsDisclaimerCoordinator mCoordinator;
    @Mock private CoordinatorFactory mCoordinatorFactory;
    @Mock private EnterpriseSignalsDisclaimerHost mHost;
    @Mock private HostFactory mHostFactory;
    @Mock private Callback<String> mShowInfoPageCallback;
    @Mock private EnterpriseSignalsDisclaimerController.Listener mListener;

    @Captor private ArgumentCaptor<Consumer<Integer>> mDismissalCallbackCaptor;
    @Captor private ArgumentCaptor<EnterpriseSignalsDisclaimerCoordinator.Delegate> mDelegateCaptor;

    private final Context mContext = RuntimeEnvironment.getApplication();
    private EnterpriseSignalsDisclaimerController mController;

    @Before
    public void setUp() {
        // Shared defaults which are unused by tests that never create a disclaimer.
        lenient()
                .when(mCoordinatorFactory.create(any(), any(), any(), anyInt(), any()))
                .thenReturn(mCoordinator);
        lenient().when(mHostFactory.create(anyInt(), any(), any())).thenReturn(mHost);

        mController =
                new EnterpriseSignalsDisclaimerController(
                        mContext, mShowInfoPageCallback, mCoordinatorFactory, mHostFactory);
    }

    private boolean show() {
        return mController.show(
                mIdentityManager, TestAccounts.MANAGED_ACCOUNT, ShownOn.STARTUP, mListener);
    }

    private void simulateHostDismissed(@DismissalCause int dismissalCause) {
        verify(mHostFactory).create(anyInt(), any(), mDismissalCallbackCaptor.capture());
        mDismissalCallbackCaptor.getValue().accept(dismissalCause);
    }

    private EnterpriseSignalsDisclaimerCoordinator.Delegate captureDelegate() {
        verify(mCoordinatorFactory)
                .create(any(), any(), any(), anyInt(), mDelegateCaptor.capture());
        return mDelegateCaptor.getValue();
    }

    @Test
    public void show_createsCoordinatorAndShowsHost() {
        when(mHost.isActive()).thenReturn(true);
        Assert.assertTrue(show());

        verify(mCoordinatorFactory)
                .create(
                        eq(mContext),
                        eq(mIdentityManager),
                        eq(TestAccounts.MANAGED_ACCOUNT),
                        anyInt(),
                        any());
        verify(mHostFactory).create(anyInt(), eq(mCoordinator), any());
        verify(mHost).show();
        Assert.assertTrue(mController.isDisclaimerActive());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void show_onPhone_usesBottomSheet() {
        show();

        verify(mCoordinatorFactory)
                .create(any(), any(), any(), eq(PresentationMode.BOTTOM_SHEET), any());
        verify(mHostFactory).create(eq(PresentationMode.BOTTOM_SHEET), any(), any());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void show_onTablet_usesModalDialog() {
        show();

        verify(mCoordinatorFactory)
                .create(any(), any(), any(), eq(PresentationMode.MODAL_DIALOG), any());
        verify(mHostFactory).create(eq(PresentationMode.MODAL_DIALOG), any(), any());
    }

    @Test
    public void show_alreadyActive_returnsFalse() {
        when(mHost.isActive()).thenReturn(true);
        show();

        Assert.assertFalse(show());
        verify(mHost).show();
    }

    @Test
    public void show_destroyed_returnsFalse() {
        mController.destroy();

        Assert.assertFalse(show());
        verifyNoInteractions(mCoordinatorFactory, mHostFactory);
    }

    @Test
    public void delegate_onAcceptAndOnDecline_dismissHost() {
        show();
        EnterpriseSignalsDisclaimerCoordinator.Delegate delegate = captureDelegate();

        delegate.onAccept();
        verify(mHost).dismiss(DismissalCause.TAPPED_ACCEPT);

        delegate.onDecline();
        verify(mHost).dismiss(DismissalCause.TAPPED_SIGN_OUT);
    }

    @Test
    public void delegate_showInfoPage_forwardsUrl() {
        show();

        captureDelegate().showInfoPage("https://example.com");

        verify(mShowInfoPageCallback).onResult("https://example.com");
    }

    @Test
    public void dismissed_accept_notifiesAcceptedAndTearsDown() {
        show();

        simulateHostDismissed(DismissalCause.TAPPED_ACCEPT);

        verify(mListener).onAccepted();
        verify(mListener, never()).onDeclined();
        verify(mHost).destroy();
        verify(mCoordinator).destroy();
        Assert.assertFalse(mController.isDisclaimerActive());
    }

    @Test
    public void dismissed_signOut_notifiesDeclined() {
        show();
        simulateHostDismissed(DismissalCause.TAPPED_SIGN_OUT);
        verify(mListener).onDeclined();
    }

    @Test
    public void dismissed_backPress_notifiesDeclined() {
        show();
        simulateHostDismissed(DismissalCause.DISMISSED_BY_BACK_PRESS);
        verify(mListener).onDeclined();
    }

    @Test
    public void dismissed_swipeDown_notifiesDeclined() {
        show();
        simulateHostDismissed(DismissalCause.DISMISSED_BY_SWIPE_DOWN);
        verify(mListener).onDeclined();
    }

    @Test
    public void dismissed_tapOutside_notifiesDeclined() {
        show();
        simulateHostDismissed(DismissalCause.DISMISSED_BY_TAP_OUTSIDE);
        verify(mListener).onDeclined();
    }

    @Test
    public void dismissed_closeButton_notifiesDeclined() {
        show();
        simulateHostDismissed(DismissalCause.DISMISSED_BY_CLOSE_BUTTON);
        verify(mListener).onDeclined();
    }

    @Test
    public void dismissed_withoutExplicitUserAction_doesNotNotify() {
        show();

        simulateHostDismissed(DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION);

        verifyNoInteractions(mListener);
        verify(mHost).destroy();
        verify(mCoordinator).destroy();
    }

    @Test
    public void destroy_whileShowing_tearsDownWithoutNotifying() {
        show();

        mController.destroy();

        verify(mHost).destroy();
        verify(mCoordinator).destroy();
        verifyNoInteractions(mListener);
    }

    @Test
    public void show_recordsShowRequested() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        MetricsHelper.HISTOGRAM_SHOWN_REQUESTED, ShownOn.SIGN_IN);

        mController.show(
                mIdentityManager, TestAccounts.MANAGED_ACCOUNT, ShownOn.SIGN_IN, mListener);

        histogramWatcher.assertExpected();
    }

    @Test
    public void show_alreadyActive_doesNotRecordShowRequested() {
        when(mHost.isActive()).thenReturn(true);
        show();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(MetricsHelper.HISTOGRAM_SHOWN_REQUESTED)
                        .build();

        Assert.assertFalse(show());

        histogramWatcher.assertExpected();
    }

    @Test
    public void delegate_onShown_recordsShown() {
        mController.show(
                mIdentityManager, TestAccounts.MANAGED_ACCOUNT, ShownOn.SIGN_IN, mListener);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        MetricsHelper.HISTOGRAM_SHOWN, ShownOn.SIGN_IN);

        captureDelegate().onShown();

        histogramWatcher.assertExpected();
    }

    @Test
    public void dismissal_afterShown_recordsResultAndTimeToUserAction() {
        show();
        captureDelegate().onShown();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT, DismissalCause.TAPPED_ACCEPT)
                        .expectAnyRecord(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION)
                        .build();

        simulateHostDismissed(DismissalCause.TAPPED_ACCEPT);

        histogramWatcher.assertExpected();
    }

    @Test
    public void dismissal_beforeShown_doesNotRecordTimeToUserAction() {
        show();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT,
                                DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION)
                        .expectNoRecords(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION)
                        .build();

        simulateHostDismissed(DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION);

        histogramWatcher.assertExpected();
    }

    @Test
    public void metricsHelper_isSharedAcrossDisclaimers() {
        EnterpriseSignalsDisclaimerHost host1 = mock(EnterpriseSignalsDisclaimerHost.class);
        EnterpriseSignalsDisclaimerHost host2 = mock(EnterpriseSignalsDisclaimerHost.class);
        when(mHostFactory.create(anyInt(), any(), any())).thenReturn(host1).thenReturn(host2);

        // The first disclaimer is implicitly dismissed.
        Assert.assertTrue(show());
        simulateLatestHostDismissed(DismissalCause.DISMISSED_BY_SWIPE_DOWN);

        // The second disclaimer is accepted, which should account for the earlier implicit
        // dismissal. This only works if the same MetricsHelper instance is used for both.
        Assert.assertTrue(show());
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        MetricsHelper.HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE, 1);
        simulateLatestHostDismissed(DismissalCause.TAPPED_ACCEPT);

        histogramWatcher.assertExpected();
    }

    /** Simulates the most recently created host reporting that it was dismissed. */
    private void simulateLatestHostDismissed(@DismissalCause int dismissalCause) {
        verify(mHostFactory, atLeastOnce())
                .create(anyInt(), any(), mDismissalCallbackCaptor.capture());
        mDismissalCallbackCaptor.getValue().accept(dismissalCause);
    }
}
