// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.activity.OnBackPressedCallback;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Consumer;

/** Unit tests for {@link ModalDialogDisclaimerHost}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ModalDialogDisclaimerHostUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Consumer<@DismissalCause Integer> mDialogDismissedCallback;
    @Captor private ArgumentCaptor<PropertyModel> mDialogModelCaptor;

    private final View mView = new View(RuntimeEnvironment.getApplication());
    private ModalDialogDisclaimerHost mHost;

    @Before
    public void setUp() {
        mHost = new ModalDialogDisclaimerHost(mModalDialogManager, mView, mDialogDismissedCallback);
        doAnswer(
                        invocation -> {
                            PropertyModel model = invocation.getArgument(0);
                            int cause = invocation.getArgument(1);
                            if (model != null
                                    && model.get(ModalDialogProperties.CONTROLLER) != null) {
                                model.get(ModalDialogProperties.CONTROLLER).onDismiss(model, cause);
                            }
                            return null;
                        })
                .when(mModalDialogManager)
                .dismissDialog(any(), anyInt());
    }

    @Test
    public void testShow_showsDialogAndSetsActive() {
        Assert.assertFalse(mHost.isActive());

        mHost.show();

        Assert.assertTrue(mHost.isActive());
        verify(mModalDialogManager)
                .showDialog(
                        any(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(ModalDialogManager.ModalDialogPriority.HIGH));
    }

    @Test
    public void testDismiss_dismissesDialogAndSetsInactive() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.dismiss(DismissalCause.TAPPED_ACCEPT);

        Assert.assertFalse(mHost.isActive());
        verify(mModalDialogManager)
                .dismissDialog(any(), eq(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED));
    }

    @Test
    public void testDismiss_invokesCallbackWithDismissalCause() {
        mHost.show();
        mHost.dismiss(DismissalCause.TAPPED_ACCEPT);

        verify(mDialogDismissedCallback).accept(DismissalCause.TAPPED_ACCEPT);
    }

    @Test
    public void testDestroy_dismissesDialogAndSetsInactive() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.destroy();

        Assert.assertFalse(mHost.isActive());
        verify(mModalDialogManager)
                .dismissDialog(any(), eq(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED));
        verify(mDialogDismissedCallback, never()).accept(any());
    }

    @Test
    public void testDestroy_calledAfterDismiss_doesNotDismissDialogAgain() {
        mHost.show();
        mHost.dismiss(DismissalCause.TAPPED_ACCEPT);

        verify(mModalDialogManager, times(1)).dismissDialog(any(), anyInt());

        mHost.destroy();

        verify(mModalDialogManager, times(1)).dismissDialog(any(), anyInt());
    }

    @Test
    public void testOnDismiss_touchOutside_invokesCallbackWithDismissedByTapOutside() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.onDismiss(null, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        Assert.assertFalse(mHost.isActive());
        verify(mDialogDismissedCallback).accept(DismissalCause.DISMISSED_BY_TAP_OUTSIDE);
    }

    @Test
    public void testBackPressedHandler_invokesCallbackWithDismissedByBackPress() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        verify(mModalDialogManager)
                .showDialog(
                        mDialogModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(ModalDialogManager.ModalDialogPriority.HIGH));
        PropertyModel dialogModel = mDialogModelCaptor.getValue();
        OnBackPressedCallback backPressCallback =
                dialogModel.get(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER);
        Assert.assertNotNull(backPressCallback);
        Assert.assertTrue(backPressCallback.isEnabled());

        backPressCallback.handleOnBackPressed();

        Assert.assertFalse(mHost.isActive());
        verify(mDialogDismissedCallback).accept(DismissalCause.DISMISSED_BY_BACK_PRESS);
        verify(mModalDialogManager)
                .dismissDialog(
                        eq(dialogModel), eq(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED));

        // Subsequent onDismiss call should not trigger the callback again.
        mHost.onDismiss(dialogModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        verify(mDialogDismissedCallback, times(1)).accept(any());
    }

    @Test
    public void testOnDismiss_otherReason_invokesCallbackWithDismissedWithoutUserAction() {
        int[] dismissalCauses = {
            DialogDismissalCause.UNKNOWN,
            DialogDismissalCause.POSITIVE_BUTTON_CLICKED,
            DialogDismissalCause.NEGATIVE_BUTTON_CLICKED,
            DialogDismissalCause.ACTION_ON_CONTENT,
            DialogDismissalCause.DISMISSED_BY_NATIVE,
            DialogDismissalCause.TAB_SWITCHED,
            DialogDismissalCause.TAB_DESTROYED,
            DialogDismissalCause.ACTIVITY_DESTROYED,
            DialogDismissalCause.NOT_ATTACHED_TO_WINDOW,
            DialogDismissalCause.NAVIGATE,
            DialogDismissalCause.NAVIGATE_BACK,
            DialogDismissalCause.TOUCH_OUTSIDE,
            DialogDismissalCause.WEB_CONTENTS_DESTROYED,
            DialogDismissalCause.DIALOG_INTERACTION_DEFERRED,
            DialogDismissalCause.ACTION_ON_DIALOG_NOT_POSSIBLE,
            DialogDismissalCause.CLIENT_TIMEOUT
        };
        for (@DialogDismissalCause int dismissalCause : dismissalCauses) {
            // Use a fresh host for each cause, since the dismissal callback is only run once.
            clearInvocations(mModalDialogManager, mDialogDismissedCallback);
            mHost =
                    new ModalDialogDisclaimerHost(
                            mModalDialogManager, mView, mDialogDismissedCallback);
            mHost.show();
            Assert.assertTrue(mHost.isActive());

            mHost.onDismiss(null, dismissalCause);

            Assert.assertFalse(mHost.isActive());
            verify(mDialogDismissedCallback)
                    .accept(DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION);
        }
    }

    @Test
    public void testOnDismiss_invokedMultipleTimes_callbackOnlyInvokedOnce() {
        mHost.show();

        mHost.onDismiss(null, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        mHost.onDismiss(null, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mDialogDismissedCallback, times(1)).accept(DismissalCause.DISMISSED_BY_TAP_OUTSIDE);
    }
}
