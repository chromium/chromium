// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Tests for {@link SigninBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
public class SigninBottomSheetCoordinatorTest {

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;
    @Mock private SigninBottomSheetCoordinator.Delegate mDelegateMock;
    @Mock private ActivityWindowAndroid mWindowAndroidMock;
    @Mock private BottomSheetController mBottomSheetControllerMock;
    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncherMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private AccountPreviewDataService mAccountPreviewDataServiceMock;
    @Mock private ModalDialogManager mModalDialogManagerMock;

    private Activity mActivity;
    private SigninBottomSheetCoordinator mCoordinator;
    private AccountPickerBottomSheetStrings mStrings;

    @Before
    public void setUp() {
        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsJniMock);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroidMock.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroidMock.getModalDialogManager()).thenReturn(mModalDialogManagerMock);
        when(mSigninManagerMock.getIdentityManager()).thenReturn(mIdentityManagerMock);

        mStrings = new AccountPickerBottomSheetStrings.Builder("Test Title").build();
        mCoordinator = new SigninBottomSheetCoordinator(mDelegateMock, FlowVariant.OTHER);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ACCOUNT_PICKER_DIALOG)
    public void testShow_onPhone_flagDisabled_createsBottomSheet() {
        mCoordinator.show(
                mWindowAndroidMock,
                mActivity,
                mModalDialogManagerMock,
                mBottomSheetControllerMock,
                mDeviceLockActivityLauncherMock,
                mSigninManagerMock,
                mAccountPreviewDataServiceMock,
                mStrings,
                AccountPickerLaunchMode.DEFAULT,
                /* isSeamlessSigninFlow= */ false,
                SigninAccessPoint.WEB_SIGNIN,
                TestAccounts.ACCOUNT1.getId());

        verify(mBottomSheetControllerMock).requestShowContent(any(), eq(true));
        verify(mModalDialogManagerMock, never()).showDialog(any(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ACCOUNT_PICKER_DIALOG)
    public void testShow_onPhone_flagEnabled_createsBottomSheet() {
        mCoordinator.show(
                mWindowAndroidMock,
                mActivity,
                mModalDialogManagerMock,
                mBottomSheetControllerMock,
                mDeviceLockActivityLauncherMock,
                mSigninManagerMock,
                mAccountPreviewDataServiceMock,
                mStrings,
                AccountPickerLaunchMode.DEFAULT,
                /* isSeamlessSigninFlow= */ false,
                SigninAccessPoint.WEB_SIGNIN,
                TestAccounts.ACCOUNT1.getId());

        verify(mBottomSheetControllerMock).requestShowContent(any(), eq(true));
        verify(mModalDialogManagerMock, never()).showDialog(any(), anyInt());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @DisableFeatures(ChromeFeatureList.ACCOUNT_PICKER_DIALOG)
    public void testShow_onTablet_flagDisabled_createsBottomSheet() {
        mCoordinator.show(
                mWindowAndroidMock,
                mActivity,
                mModalDialogManagerMock,
                mBottomSheetControllerMock,
                mDeviceLockActivityLauncherMock,
                mSigninManagerMock,
                mAccountPreviewDataServiceMock,
                mStrings,
                AccountPickerLaunchMode.DEFAULT,
                /* isSeamlessSigninFlow= */ false,
                SigninAccessPoint.WEB_SIGNIN,
                TestAccounts.ACCOUNT1.getId());

        verify(mBottomSheetControllerMock).requestShowContent(any(), eq(true));
        verify(mModalDialogManagerMock, never()).showDialog(any(), anyInt());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ACCOUNT_PICKER_DIALOG)
    public void testShow_onTablet_flagEnabled_createsModalDialog() {
        mCoordinator.show(
                mWindowAndroidMock,
                mActivity,
                mModalDialogManagerMock,
                mBottomSheetControllerMock,
                mDeviceLockActivityLauncherMock,
                mSigninManagerMock,
                mAccountPreviewDataServiceMock,
                mStrings,
                AccountPickerLaunchMode.DEFAULT,
                /* isSeamlessSigninFlow= */ false,
                SigninAccessPoint.WEB_SIGNIN,
                TestAccounts.ACCOUNT1.getId());

        verify(mModalDialogManagerMock)
                .showDialog(any(), eq(ModalDialogManager.ModalDialogType.APP));
        verify(mBottomSheetControllerMock, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ACCOUNT_PICKER_DIALOG)
    public void testDismissModalDialog_onTablet_flagEnabled_dismissesDialog() {
        mCoordinator.show(
                mWindowAndroidMock,
                mActivity,
                mModalDialogManagerMock,
                mBottomSheetControllerMock,
                mDeviceLockActivityLauncherMock,
                mSigninManagerMock,
                mAccountPreviewDataServiceMock,
                mStrings,
                AccountPickerLaunchMode.DEFAULT,
                /* isSeamlessSigninFlow= */ false,
                SigninAccessPoint.WEB_SIGNIN,
                TestAccounts.ACCOUNT1.getId());

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManagerMock)
                .showDialog(modelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));

        mCoordinator.destroy();

        verify(mModalDialogManagerMock)
                .dismissDialog(
                        eq(modelCaptor.getValue()), eq(DialogDismissalCause.ACTION_ON_CONTENT));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ACCOUNT_PICKER_DIALOG)
    public void testModalDialogDismissal_navigateBack_logsMetricAndCancelsSignIn() {
        mCoordinator.show(
                mWindowAndroidMock,
                mActivity,
                mModalDialogManagerMock,
                mBottomSheetControllerMock,
                mDeviceLockActivityLauncherMock,
                mSigninManagerMock,
                mAccountPreviewDataServiceMock,
                mStrings,
                AccountPickerLaunchMode.DEFAULT,
                /* isSeamlessSigninFlow= */ false,
                SigninAccessPoint.WEB_SIGNIN,
                TestAccounts.ACCOUNT1.getId());

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManagerMock)
                .showDialog(modelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));

        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);
        controller.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.DISMISSED_BACK, SigninAccessPoint.WEB_SIGNIN);
        verify(mDelegateMock).onSignInCancel();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ACCOUNT_PICKER_DIALOG)
    public void testModalDialogDismissal_actionOnContent_doesNotLogDismissedBackNorCancelSignIn() {
        mCoordinator.show(
                mWindowAndroidMock,
                mActivity,
                mModalDialogManagerMock,
                mBottomSheetControllerMock,
                mDeviceLockActivityLauncherMock,
                mSigninManagerMock,
                mAccountPreviewDataServiceMock,
                mStrings,
                AccountPickerLaunchMode.DEFAULT,
                /* isSeamlessSigninFlow= */ false,
                SigninAccessPoint.WEB_SIGNIN,
                TestAccounts.ACCOUNT1.getId());

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManagerMock)
                .showDialog(modelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));

        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);
        controller.onDismiss(model, DialogDismissalCause.ACTION_ON_CONTENT);

        verify(mSigninMetricsUtilsJniMock, never())
                .logAccountConsistencyPromoAction(
                        eq(AccountConsistencyPromoAction.DISMISSED_BACK), anyInt());
        verify(mDelegateMock, never()).onSignInCancel();
    }
}
