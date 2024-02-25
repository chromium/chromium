// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link SigninUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(SigninFeatures.ENTERPRISE_POLICY_ON_SIGNIN)
public class SigninUtilsTest {
    @Rule public final TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private SigninManager mSigninManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Context mContext;

    private CoreAccountInfo mCoreAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId("email@domain.com", "gaia-id");
    private SignInCallback mCallback;
    private boolean mSignInCompleted;
    private boolean mSignInAborted;
    private boolean mIsAccountManaged;

    @Before
    public void setUp() {
        when(mSigninManager.extractDomainName(mCoreAccountInfo.getEmail()))
                .thenReturn("domain.com");

        mCallback =
                new SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        mSignInCompleted = true;
                    }

                    @Override
                    public void onSignInAborted() {
                        mSignInAborted = true;
                    }
                };

        doAnswer(
                        (args) -> {
                            ((Callback<Boolean>) args.getArgument(1)).onResult(mIsAccountManaged);
                            return null;
                        })
                .when(mSigninManager)
                .isAccountManaged(eq(mCoreAccountInfo), any());
    }

    @Test
    @DisableFeatures(SigninFeatures.ENTERPRISE_POLICY_ON_SIGNIN)
    public void testPolicyOnSigninDisabled() {
        @SigninAccessPoint int accessPoint = SigninAccessPoint.START_PAGE;
        SigninUtils.checkAccountManagementAndSignIn(
                mCoreAccountInfo, mSigninManager, accessPoint, null, mContext, mModalDialogManager);

        verify(mSigninManager).signin(eq(mCoreAccountInfo), eq(accessPoint), isNull());
        verify(mSigninManager, never()).setUserAcceptedAccountManagement(anyBoolean());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }

    @Test
    public void testAccountManagementAccepted_SigninSucceeded() {
        mIsAccountManaged = true;
        @SigninAccessPoint int accessPoint = SigninAccessPoint.START_PAGE;
        doAnswer(
                        (args) -> {
                            PropertyModel propertyModel = args.getArgument(0);
                            Controller controller =
                                    propertyModel.get(ModalDialogProperties.CONTROLLER);
                            controller.onClick(
                                    propertyModel, ModalDialogProperties.ButtonType.POSITIVE);
                            return null;
                        })
                .when(mModalDialogManager)
                .showDialog(any(), anyInt());

        doAnswer(
                        (args) -> {
                            SignInCallback callback = args.getArgument(2);
                            callback.onSignInComplete();
                            return null;
                        })
                .when(mSigninManager)
                .signin(eq(mCoreAccountInfo), eq(accessPoint), notNull());

        SigninUtils.checkAccountManagementAndSignIn(
                mCoreAccountInfo,
                mSigninManager,
                accessPoint,
                mCallback,
                mContext,
                mModalDialogManager);
        assertTrue(mSignInCompleted);
        assertFalse(mSignInAborted);
        verify(mSigninManager).setUserAcceptedAccountManagement(true);
    }

    @Test
    public void testAccountManagementAccepted_SigninAborted() {
        mIsAccountManaged = true;
        InOrder inOrder = inOrder(mSigninManager);
        @SigninAccessPoint int accessPoint = SigninAccessPoint.START_PAGE;
        doAnswer(
                        (args) -> {
                            PropertyModel propertyModel = args.getArgument(0);
                            Controller controller =
                                    propertyModel.get(ModalDialogProperties.CONTROLLER);
                            controller.onClick(
                                    propertyModel, ModalDialogProperties.ButtonType.POSITIVE);
                            return null;
                        })
                .when(mModalDialogManager)
                .showDialog(any(), anyInt());

        doAnswer(
                        (args) -> {
                            SignInCallback callback = args.getArgument(2);
                            callback.onSignInAborted();
                            return null;
                        })
                .when(mSigninManager)
                .signin(eq(mCoreAccountInfo), eq(accessPoint), notNull());

        SigninUtils.checkAccountManagementAndSignIn(
                mCoreAccountInfo,
                mSigninManager,
                accessPoint,
                mCallback,
                mContext,
                mModalDialogManager);
        assertFalse(mSignInCompleted);
        assertTrue(mSignInAborted);
        inOrder.verify(mSigninManager).setUserAcceptedAccountManagement(true);
        inOrder.verify(mSigninManager).setUserAcceptedAccountManagement(false);
    }

    @Test
    public void testAccountManagementRejected() {
        mIsAccountManaged = true;
        @SigninAccessPoint int accessPoint = SigninAccessPoint.START_PAGE;
        doAnswer(
                        (args) -> {
                            PropertyModel propertyModel = args.getArgument(0);
                            Controller controller =
                                    propertyModel.get(ModalDialogProperties.CONTROLLER);
                            controller.onDismiss(
                                    propertyModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                            return null;
                        })
                .when(mModalDialogManager)
                .showDialog(any(), anyInt());

        SigninUtils.checkAccountManagementAndSignIn(
                mCoreAccountInfo,
                mSigninManager,
                accessPoint,
                mCallback,
                mContext,
                mModalDialogManager);
        assertFalse(mSignInCompleted);
        assertTrue(mSignInAborted);
        verify(mSigninManager, never()).setUserAcceptedAccountManagement(true);
        verify(mSigninManager, never()).signin(any(), anyInt(), any());
    }

    @Test
    public void testAccountNotManaged() {
        mIsAccountManaged = false;
        @SigninAccessPoint int accessPoint = SigninAccessPoint.START_PAGE;

        SigninUtils.checkAccountManagementAndSignIn(
                mCoreAccountInfo,
                mSigninManager,
                accessPoint,
                mCallback,
                mContext,
                mModalDialogManager);
        verify(mSigninManager).signin(eq(mCoreAccountInfo), eq(accessPoint), eq(mCallback));
        verify(mSigninManager, never()).setUserAcceptedAccountManagement(anyBoolean());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }
}
