// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
import org.chromium.chrome.browser.signin.services.BadgeConfig;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.ForcedSigninStatusProvider;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator.SigninFlow;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator.Delegate;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninMediator.LoadPoint;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SigninFunnelEvent;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Collection;

/**
 * Unit tests for {@link FullscreenSigninMediator}.
 *
 * <p>TODO(crbug.com/493130564): Revert to regular runner after
 * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS launch.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class FullscreenSigninMediatorTest {
    @Rule(order = Rule.DEFAULT_ORDER - 1)
    public final BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Parameters(name = "{index}_isIdentityMgr={0}")
    public static Collection parameters() {
        return Arrays.asList(false, true);
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private Delegate mDelegateMock;
    @Mock private ModalDialogManager mModalDialogManagerMock;
    @Mock private PrivacyPreferencesManager mPrivacyPreferencesManagerMock;
    @Mock private ProfileProvider mProfileProviderMock;
    @Mock private Profile mProfileMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private AccountPreviewDataService mAccountPreviewDataServiceMock;
    @Mock private ForcedSigninStatusProvider mForcedSigninStatusProviderMock;
    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;
    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private SyncService mSyncServiceMock;

    private final OneshotSupplierImpl<ProfileProvider> mProfileSupplier =
            new OneshotSupplierImpl<>();
    private OneshotSupplierImpl<Boolean> mPolicyLoadListener = new OneshotSupplierImpl<>();
    private OneshotSupplierImpl<Boolean> mChildAccountStatusSupplier = new OneshotSupplierImpl<>();
    private final Promise<Void> mNativeInitializationPromise = new Promise<>();
    private final boolean mIsIdentityManagerSourceOfAccounts;

    private FullscreenSigninMediator mMediator;
    private PropertyModel mModel;

    // Config used by createMediator(). Tests can change these before calling it.
    private int mLogoId;
    private String mSelectedAccountEmail;

    public FullscreenSigninMediatorTest(boolean isIdentityManagerSourceOfAccounts) {
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
                mIsIdentityManagerSourceOfAccounts);

        IdentityServicesProvider.setSigninManagerForTesting(mSigninManagerMock);
        IdentityServicesProvider.setAccountPreviewDataServiceForTesting(
                mAccountPreviewDataServiceMock);
        ForcedSigninStatusProvider.setInstanceForTesting(mForcedSigninStatusProviderMock);
        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsJniMock);

        lenient().when(mProfileProviderMock.getOriginalProfile()).thenReturn(mProfileMock);
        mProfileSupplier.set(mProfileProviderMock);

        lenient().when(mDelegateMock.getProfileSupplier()).thenReturn(mProfileSupplier);
        when(mDelegateMock.getNativeInitializationPromise())
                .thenReturn(mNativeInitializationPromise);
        when(mDelegateMock.getPolicyLoadListener()).thenReturn(mPolicyLoadListener);
        when(mDelegateMock.getChildAccountStatusSupplier()).thenReturn(mChildAccountStatusSupplier);

        // Policies and child account status are ready, while native initialization is fulfilled in
        // initializeNative().
        mPolicyLoadListener.set(false);
        mChildAccountStatusSupplier.set(false);
    }

    @After
    public void tearDown() {
        destroyMediator();
    }

    @Test
    public void testInitialLoadWaitsForNativeInitialization() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        createMediator();
        assertLoadingSpinnerVisibility(true);
        assertNoAccountIsDisplayed();
        verify(mDelegateMock, never()).recordLoadCompletedHistograms(anyInt());

        initializeNative();
        assertLoadingSpinnerVisibility(false);
        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT1);
        verify(mDelegateMock).recordLoadCompletedHistograms(LoadPoint.NATIVE_INITIALIZATION);
    }

    @Test
    public void testDestroyBeforeNativeInitialization() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();

        destroyMediator();
        initializeNative();

        assertLoadingSpinnerVisibility(true);
        assertNoAccountIsDisplayed();
        verify(mDelegateMock, never()).recordLoadCompletedHistograms(anyInt());
        verify(mDelegateMock, never()).recordNativeInitializedHistogram();
    }

    @Test
    public void testInitialLoadWaitsForAccounts() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        try (var blocker = mAccountManagerTestRule.blockGetAccountsUpdate()) {
            createMediator();
            initializeNative();
            assertLoadingSpinnerVisibility(true);
            assertNoAccountIsDisplayed();
            verify(mDelegateMock, never()).recordLoadCompletedHistograms(anyInt());
        }
        RobolectricUtil.runAllBackgroundAndUi();

        assertLoadingSpinnerVisibility(false);
        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT1);
        verify(mDelegateMock).recordLoadCompletedHistograms(LoadPoint.ACCOUNT_FETCHING);
    }

    @Test
    public void testInitialLoadWaitsForPolicyLoad() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mPolicyLoadListener = new OneshotSupplierImpl<>();
        when(mDelegateMock.getPolicyLoadListener()).thenReturn(mPolicyLoadListener);

        createMediator();
        initializeNative();
        assertLoadingSpinnerVisibility(true);
        verify(mDelegateMock, never()).recordLoadCompletedHistograms(anyInt());

        mPolicyLoadListener.set(false);
        RobolectricUtil.runAllBackgroundAndUi();

        assertLoadingSpinnerVisibility(false);
        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT1);
        verify(mDelegateMock).recordLoadCompletedHistograms(LoadPoint.POLICY_LOAD);
    }

    @Test
    public void testInitialLoadWaitsForChildAccountStatusLoad() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mChildAccountStatusSupplier = new OneshotSupplierImpl<>();
        when(mDelegateMock.getChildAccountStatusSupplier()).thenReturn(mChildAccountStatusSupplier);

        createMediator();
        initializeNative();
        assertLoadingSpinnerVisibility(true);
        verify(mDelegateMock, never()).recordLoadCompletedHistograms(anyInt());

        mChildAccountStatusSupplier.set(false);
        RobolectricUtil.runAllBackgroundAndUi();

        assertLoadingSpinnerVisibility(false);
        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT1);
        verify(mDelegateMock).recordLoadCompletedHistograms(LoadPoint.CHILD_STATUS_LOAD);
    }

    @Test
    public void testProfileDataUpdateUpdatesSelectedAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();
        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT1);

        AccountInfo updatedAccount =
                new AccountInfo.Builder(TestAccounts.ACCOUNT1).fullName("Updated Name").build();
        mAccountManagerTestRule.updateAccount(updatedAccount);
        RobolectricUtil.runAllBackgroundAndUi();

        assertSelectedAccountIsDisplayed(updatedAccount);
    }

    @Test
    public void testAccountRemovalClearsSelectedAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();
        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT1);

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        RobolectricUtil.runAllBackgroundAndUi();

        assertNoAccountIsDisplayed();
        assertFalse(mModel.get(FullscreenSigninProperties.ENABLE_ACCOUNT_SELECTION));
    }

    @Test
    public void testChildBadgeIsSetForChildAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);
        createMediator();
        initializeNative();

        assertSelectedAccountIsDisplayed(TestAccounts.CHILD_ACCOUNT);
        assertTrue(mModel.get(FullscreenSigninProperties.SHOW_ACCOUNT_SUPERVISION_NOTICE));
        Context context = RuntimeEnvironment.getApplication();
        assertEquals(
                BadgeConfig.create(R.drawable.ic_account_child_20dp)
                        .withDefaultSizeChildAccountConfig()
                        .build(context),
                mMediator.getContinueButtonBadgeConfigForTesting());
        assertEquals(
                BadgeConfig.create(R.drawable.ic_account_child_40dp)
                        .withLargeChildAccountConfig()
                        .build(context),
                mMediator.getSigninAnimationBadgeConfigForTesting());
    }

    @Test
    public void testChildBadgeIsRemovedWhenAccountStopsBeingSupervised() {
        mAccountManagerTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);
        createMediator();
        initializeNative();
        assertSelectedAccountIsDisplayed(TestAccounts.CHILD_ACCOUNT);
        assertTrue(mModel.get(FullscreenSigninProperties.SHOW_ACCOUNT_SUPERVISION_NOTICE));
        assertNotNull(mMediator.getContinueButtonBadgeConfigForTesting());
        assertNotNull(mMediator.getSigninAnimationBadgeConfigForTesting());

        // The account stops being supervised while the screen is shown. The child account status
        // is re-checked on account list updates, simulate the resulting callback.
        AccountInfo unsupervisedAccount =
                new AccountInfo.Builder(TestAccounts.CHILD_ACCOUNT)
                        .accountCapabilities(
                                new AccountCapabilitiesBuilder()
                                        .setIsSubjectToParentalControls(false)
                                        .build())
                        .build();
        mAccountManagerTestRule.updateAccount(unsupervisedAccount);
        mMediator.onChildAccountStatusReady(/* isChild= */ false, /* childInfo= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        assertSelectedAccountIsDisplayed(unsupervisedAccount);
        assertFalse(mModel.get(FullscreenSigninProperties.SHOW_ACCOUNT_SUPERVISION_NOTICE));
        assertNull(mMediator.getContinueButtonBadgeConfigForTesting());
        assertNull(mMediator.getSigninAnimationBadgeConfigForTesting());
    }

    @Test
    public void testDestroyStopsObservingAccounts() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        destroyMediator();
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        RobolectricUtil.runAllBackgroundAndUi();

        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT1);
    }

    @Test
    public void testChildAccountStatusReadyAfterDestroy() {
        mAccountManagerTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);
        createMediator();
        initializeNative();
        FullscreenSigninMediator mediator = mMediator;

        destroyMediator();
        lenient().when(mDelegateMock.getProfileSupplier()).thenReturn(null);
        mediator.onChildAccountStatusReady(/* isChild= */ false, /* childInfo= */ null);

        assertTrue(mModel.get(FullscreenSigninProperties.SHOW_ACCOUNT_SUPERVISION_NOTICE));
    }

    @Test
    public void testSignInWithDefaultLogoWaitsForAnimation() {
        // Skip the account management check, so that sign-in is requested directly.
        when(mSigninManagerMock.getUserAcceptedAccountManagement()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        mMediator.proceedWithSignIn();

        assertTrue(mModel.get(FullscreenSigninProperties.START_ANIMATION));
        AnimatorListener listener = mModel.get(FullscreenSigninProperties.ANIMATOR_LISTENER);
        assertNotNull(listener);
        // Sign-in doesn't continue until the animation ends.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mSigninManagerMock, never()).signin(any(), anyInt(), any());

        listener.onAnimationEnd(mock(Animator.class));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mSigninManagerMock).signin(eq(TestAccounts.ACCOUNT1), anyInt(), any());
    }

    @Test
    public void testSignInWithCustomLogoCompletesWithoutAnimation() {
        // Skip the account management check, so that sign-in is requested directly.
        when(mSigninManagerMock.getUserAcceptedAccountManagement()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mLogoId = R.drawable.ic_domain;
        createMediator();
        initializeNative();

        mMediator.proceedWithSignIn();

        // The animation view is hidden when a custom logo is used, so the animation isn't started.
        assertFalse(mModel.get(FullscreenSigninProperties.SHOW_ANIMATION));
        assertFalse(mModel.get(FullscreenSigninProperties.START_ANIMATION));
        assertNull(mModel.get(FullscreenSigninProperties.ANIMATOR_LISTENER));
        verify(mSigninManagerMock, never()).signin(any(), anyInt(), any());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mSigninManagerMock).signin(eq(TestAccounts.ACCOUNT1), anyInt(), any());
    }

    @Test
    public void testSigninFunnel_promoShownWithAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSigninSupported(true);
        createMediator();
        verify(mSigninMetricsUtilsJniMock, never()).logSigninFunnelEvent(anyInt(), anyInt());

        initializeNative();

        verify(mSigninMetricsUtilsJniMock)
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.PROMO_SHOWN_WITH_ACCOUNT);
    }

    @Test
    public void testSigninFunnel_promoShownNoAccount() {
        setUpSigninSupported(true);
        createMediator();

        initializeNative();

        verify(mSigninMetricsUtilsJniMock)
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.PROMO_SHOWN_NO_ACCOUNT);
    }

    @Test
    public void testSigninFunnel_notLoggedWhenSigninNotSupported() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSigninSupported(false);
        createMediator();

        initializeNative();
        mMediator.reset();

        verify(mSigninMetricsUtilsJniMock, never()).logSigninFunnelEvent(anyInt(), anyInt());
        verify(mDelegateMock, never()).logPromoShownFunnelEventOnBackPress(anyInt(), anyInt());
    }

    @Test
    public void testSigninFunnel_notLoggedAfterDestroy() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSigninSupported(true);
        createMediator();
        destroyMediator();

        initializeNative();

        verify(mSigninMetricsUtilsJniMock, never()).logSigninFunnelEvent(anyInt(), anyInt());
    }

    @Test
    public void testSigninFunnel_promoShownLoggedAgainOnReset_coordinatorReused() {
        // Simulates a flow that reuses the coordinator on back press, like the FRE.
        doAnswer(
                        invocation -> {
                            SigninMetricsUtils.logSigninFunnelEvent(
                                    invocation.getArgument(0), invocation.getArgument(1));
                            return null;
                        })
                .when(mDelegateMock)
                .logPromoShownFunnelEventOnBackPress(anyInt(), anyInt());
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSigninSupported(true);
        createMediator();
        initializeNative();

        mMediator.reset();

        verify(mSigninMetricsUtilsJniMock, times(2))
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.PROMO_SHOWN_WITH_ACCOUNT);
    }

    @Test
    public void testSigninFunnel_promoShownNotLoggedAgainOnReset_coordinatorRecreated() {
        // Simulates a flow that recreates the coordinator on back press, like the fullscreen
        // sign-in promo, where the delegate doesn't log the event again.
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSigninSupported(true);
        createMediator();
        initializeNative();

        mMediator.reset();

        verify(mSigninMetricsUtilsJniMock, times(1))
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.PROMO_SHOWN_WITH_ACCOUNT);
    }

    @Test
    public void testPendingChildAccountShowsSupervisedUi() {
        // The selected account isn't on the device yet, so it becomes a pending selection.
        setUpSigninSupported(true);
        mSelectedAccountEmail = TestAccounts.CHILD_ACCOUNT.getEmail();
        createMediator();
        initializeNative();
        assertNoAccountIsDisplayed();
        assertFalse(mModel.get(FullscreenSigninProperties.SHOW_ACCOUNT_SUPERVISION_NOTICE));

        // The parental check needs the account in AccountManagerFacade first.
        mAccountManagerTestRule.getAccountManagerFacade().addAccount(TestAccounts.CHILD_ACCOUNT);
        mAccountManagerTestRule
                .getIdentityManager()
                .addOrUpdateExtendedAccountInfo(TestAccounts.CHILD_ACCOUNT);
        RobolectricUtil.runAllBackgroundAndUi();

        assertSelectedAccountIsDisplayed(TestAccounts.CHILD_ACCOUNT);
        assertTrue(mModel.get(FullscreenSigninProperties.SHOW_ACCOUNT_SUPERVISION_NOTICE));
        assertNull(mModel.get(FullscreenSigninProperties.SUBTITLE_STRING));
        assertTrue(mModel.get(FullscreenSigninProperties.SHOULD_HIDE_DISMISS_BUTTON));
        assertFalse(mModel.get(FullscreenSigninProperties.ENABLE_ACCOUNT_SELECTION));
    }

    private void setUpSigninSupported(boolean isSigninSupported) {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(isSigninSupported);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        if (!isSigninSupported) return;

        UserPrefs.setPrefServiceForTesting(mPrefServiceMock);
        lenient().when(mPrefServiceMock.getBoolean(Pref.SIGNIN_ALLOWED)).thenReturn(true);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
    }

    private void createMediator() {
        mMediator =
                new FullscreenSigninMediator(
                        RuntimeEnvironment.getApplication(),
                        mModalDialogManagerMock,
                        mDelegateMock,
                        mPrivacyPreferencesManagerMock,
                        new FullscreenSigninConfig(
                                /* title= */ "title",
                                /* subtitle= */ "subtitle",
                                /* dismissText= */ "dismiss",
                                mLogoId,
                                /* shouldDisableSignin= */ false,
                                /* surveyType= */ null,
                                mSelectedAccountEmail,
                                SigninFlow.DEFAULT_SIGNIN),
                        SigninAccessPoint.START_PAGE);
        mModel = mMediator.getModel();
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private void destroyMediator() {
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
    }

    private void initializeNative() {
        mNativeInitializationPromise.fulfill(null);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private void assertLoadingSpinnerVisibility(boolean isVisible) {
        assertEquals(
                isVisible,
                mModel.get(FullscreenSigninProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER));
    }

    private void assertNoAccountIsDisplayed() {
        assertNull(mModel.get(FullscreenSigninProperties.BOTTOM_GROUP_ACCOUNT_DATA));
    }

    private void assertSelectedAccountIsDisplayed(AccountInfo accountInfo) {
        DisplayableProfileData selectedAccount =
                mModel.get(FullscreenSigninProperties.BOTTOM_GROUP_ACCOUNT_DATA);
        assertNotNull(selectedAccount);
        assertEquals(accountInfo.getId(), selectedAccount.getAccountId());
        assertEquals(accountInfo.getEmail(), selectedAccount.getAccountEmail());
        assertEquals(accountInfo.getFullName(), selectedAccount.getFullName());
    }
}
