// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.params.BlockJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

/** Unit tests for {@link ModalDialogDisclaimerHost}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
public class ModalDialogDisclaimerHostUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private EnterpriseSignalsDisclaimerView mView;
    @Mock private Consumer<Boolean> mDialogDismissedCallback;

    private ModalDialogDisclaimerHost mHost;

    public static class OtherDismissalCausesParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(DialogDismissalCause.UNKNOWN).name("Unknown"),
                    new ParameterSet()
                            .value(DialogDismissalCause.POSITIVE_BUTTON_CLICKED)
                            .name("PositiveButtonClicked"),
                    new ParameterSet()
                            .value(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED)
                            .name("NegativeButtonClicked"),
                    new ParameterSet()
                            .value(DialogDismissalCause.ACTION_ON_CONTENT)
                            .name("ActionOnContent"),
                    new ParameterSet()
                            .value(DialogDismissalCause.DISMISSED_BY_NATIVE)
                            .name("DismissedByNative"),
                    new ParameterSet()
                            .value(DialogDismissalCause.NAVIGATE_BACK)
                            .name("NavigateBack"),
                    new ParameterSet()
                            .value(DialogDismissalCause.TOUCH_OUTSIDE)
                            .name("TouchOutside"),
                    new ParameterSet().value(DialogDismissalCause.TAB_SWITCHED).name("TabSwitched"),
                    new ParameterSet()
                            .value(DialogDismissalCause.TAB_DESTROYED)
                            .name("TabDestroyed"),
                    new ParameterSet()
                            .value(DialogDismissalCause.ACTIVITY_DESTROYED)
                            .name("ActivityDestroyed"),
                    new ParameterSet()
                            .value(DialogDismissalCause.NOT_ATTACHED_TO_WINDOW)
                            .name("NotAttachedToWindow"),
                    new ParameterSet().value(DialogDismissalCause.NAVIGATE).name("Navigate"),
                    new ParameterSet()
                            .value(DialogDismissalCause.WEB_CONTENTS_DESTROYED)
                            .name("WebContentsDestroyed"),
                    new ParameterSet()
                            .value(DialogDismissalCause.DIALOG_INTERACTION_DEFERRED)
                            .name("DialogInteractionDeferred"),
                    new ParameterSet()
                            .value(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED)
                            .name("ActionOnDialogCompleted"),
                    new ParameterSet()
                            .value(DialogDismissalCause.ACTION_ON_DIALOG_NOT_POSSIBLE)
                            .name("ActionOnDialogNotPossible"),
                    new ParameterSet()
                            .value(DialogDismissalCause.CLIENT_TIMEOUT)
                            .name("ClientTimeout"));
        }
    }

    @Before
    public void setUp() {
        mHost = new ModalDialogDisclaimerHost(mModalDialogManager, mView, mDialogDismissedCallback);
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
    public void testHide_dismissesDialogAndSetsInactive() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.hide();

        Assert.assertFalse(mHost.isActive());
        verify(mModalDialogManager)
                .dismissDialog(any(), eq(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED));
    }

    @Test
    public void testDestroy_dismissesDialogAndSetsInactive() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.destroy();

        Assert.assertFalse(mHost.isActive());
        verify(mModalDialogManager)
                .dismissDialog(any(), eq(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED));
    }

    @Test
    public void testOnDismiss_touchOutsideReason_invokesCallbackWithTrue() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.onDismiss(null, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        Assert.assertFalse(mHost.isActive());
        verify(mDialogDismissedCallback).accept(true);
    }

    @Test
    @UseMethodParameter(OtherDismissalCausesParams.class)
    public void testOnDismiss_otherReason_invokesCallbackWithFalse(
            @DialogDismissalCause int dismissalCause) {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.onDismiss(null, dismissalCause);

        Assert.assertFalse(mHost.isActive());
        verify(mDialogDismissedCallback).accept(false);
    }

    @Test
    public void testOnDismiss_invokedMultipleTimes_callbackOnlyInvokedOnce() {
        mHost.show();

        mHost.onDismiss(null, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        mHost.onDismiss(null, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mDialogDismissedCallback, times(1)).accept(true);
    }
}
