// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.content.Context;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.prefs.LocalStatePrefsJni;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
import org.chromium.chrome.browser.signin.services.BadgeConfig;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.ForcedSigninStatusProvider;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator.SigninFlow;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator.Delegate;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SigninFunnelEvent;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
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

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

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
    @Mock private LocalStatePrefs.Natives mLocalStatePrefsNativeMock;
    @Mock private PrefService mLocalPrefsServiceMock;
    @Captor private ArgumentCaptor<SignInCallback> mSignInCallbackCaptor;
    @Captor private ArgumentCaptor<Runnable> mSignOutCallbackCaptor;

    private final OneshotSupplierImpl<ProfileProvider> mProfileSupplier =
            new OneshotSupplierImpl<>();
    private OneshotSupplierImpl<Boolean> mPolicyLoadListener = new OneshotSupplierImpl<>();
    private OneshotSupplierImpl<Boolean> mChildAccountStatusSupplier = new OneshotSupplierImpl<>();
    private final Promise<Void> mNativeInitializationPromise = new Promise<>();
    private final boolean mIsIdentityManagerSourceOfAccounts;

    private FullscreenSigninCoordinator.Mediator mMediator;
    private PropertyModel mModel;

    // Config used by createMediator(). Tests can change these before calling it.
    private int mLogoId;
    private String mSelectedAccountEmail;

    public FullscreenSigninMediatorTest(boolean isIdentityManagerSourceOfAccounts) {
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @Before
    public void setUp() {
        // The account picker tests inflate real account rows, which need a theme and read
        // SMART_EMAIL_LINE_BREAKING.
        RuntimeEnvironment.getApplication().setTheme(R.style.Theme_BrowserUI_DayNight);
        FeatureOverrides.enable(SigninFeatures.SMART_EMAIL_LINE_BREAKING);
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
    public void testInitialLoadWhenAllInputsReadyAtConstruction_doesNotRecordLoadHistograms() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mNativeInitializationPromise.fulfill(null);

        createMediator();

        assertLoadingSpinnerVisibility(false);
        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT1);
        verify(mDelegateMock, times(1)).onInitialLoadCompleted();
        verify(mDelegateMock, never()).recordNativeInitializedHistogram();
        verify(mDelegateMock, never()).recordLoadCompletedHistograms(anyInt());
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
        verify(mDelegateMock)
                .recordLoadCompletedHistograms(FullscreenSigninLoadPoint.NATIVE_INITIALIZATION);
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
        verify(mDelegateMock)
                .recordLoadCompletedHistograms(FullscreenSigninLoadPoint.ACCOUNT_FETCHING);
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
        verify(mDelegateMock).recordLoadCompletedHistograms(FullscreenSigninLoadPoint.POLICY_LOAD);
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
        verify(mDelegateMock)
                .recordLoadCompletedHistograms(FullscreenSigninLoadPoint.CHILD_STATUS_LOAD);
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
    public void testSignInWithChildAccount_usesForcedSigninAccessPoint() {
        setUpSigninSupported(true);
        // Skip the account management check, so that sign-in is requested directly.
        when(mSigninManagerMock.getUserAcceptedAccountManagement()).thenReturn(true);
        FullscreenSigninCoordinator.disableAnimationsForTesting();
        mAccountManagerTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);
        createMediator();
        initializeNative();

        // Child accounts can't skip sign-in, so they sign in with FORCED_SIGNIN rather than the
        // configured access point.
        assertTrue(mModel.get(FullscreenSigninProperties.SHOULD_HIDE_DISMISS_BUTTON));
        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);
        verify(mSigninManagerMock)
                .signin(eq(TestAccounts.CHILD_ACCOUNT), eq(SigninAccessPoint.FORCED_SIGNIN), any());
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
        captureChildAccountStatusListener()
                .onStatusReady(/* isChild= */ false, /* childAccount= */ null);
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
        ChildAccountStatusListener listener = captureChildAccountStatusListener();

        destroyMediator();
        lenient().when(mDelegateMock.getProfileSupplier()).thenReturn(null);
        listener.onStatusReady(/* isChild= */ false, /* childAccount= */ null);

        assertTrue(mModel.get(FullscreenSigninProperties.SHOW_ACCOUNT_SUPERVISION_NOTICE));
    }

    @Test
    public void testSignInWithDefaultLogoWaitsForAnimation() {
        setUpSigninSupported(true);
        // Skip the account management check, so that sign-in is requested directly.
        when(mSigninManagerMock.getUserAcceptedAccountManagement()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);

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
        setUpSigninSupported(true);
        // Skip the account management check, so that sign-in is requested directly.
        when(mSigninManagerMock.getUserAcceptedAccountManagement()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mLogoId = R.drawable.ic_domain;
        createMediator();
        initializeNative();

        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);

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
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
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
        setUpSigninSupported(false);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();

        initializeNative();
        mMediator.reset();

        verify(mSigninMetricsUtilsJniMock, never()).logSigninFunnelEvent(anyInt(), anyInt());
        verify(mDelegateMock, never()).logPromoShownFunnelEventOnBackPress(anyInt(), anyInt());
    }

    @Test
    public void testSigninFunnel_notLoggedAfterDestroy() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        destroyMediator();

        initializeNative();

        verify(mSigninMetricsUtilsJniMock, never()).logSigninFunnelEvent(anyInt(), anyInt());
    }

    @Test
    public void testSigninFunnel_promoShownLoggedAgainOnReset_coordinatorReused() {
        setUpSigninSupported(true);
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
        createMediator();
        initializeNative();

        mMediator.reset();

        verify(mSigninMetricsUtilsJniMock, times(2))
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.PROMO_SHOWN_WITH_ACCOUNT);
    }

    @Test
    public void testSigninFunnel_promoShownNotLoggedAgainOnReset_coordinatorRecreated() {
        setUpSigninSupported(true);
        // Simulates a flow that recreates the coordinator on back press, like the fullscreen
        // sign-in promo, where the delegate doesn't log the event again.
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        mMediator.reset();

        verify(mSigninMetricsUtilsJniMock, times(1))
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.PROMO_SHOWN_WITH_ACCOUNT);
    }

    @Test
    public void testSigninFunnel_resetWithNoAccount_notifiesDelegate() {
        setUpSigninSupported(true);
        createMediator();
        initializeNative();

        mMediator.reset();

        verify(mDelegateMock)
                .logPromoShownFunnelEventOnBackPress(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.PROMO_SHOWN_NO_ACCOUNT);
    }

    @Test
    public void testPendingChildAccountShowsSupervisedUi() {
        setUpSigninSupported(true);
        // The selected account isn't on the device yet, so it becomes a pending selection.
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

    @Test
    public void testSigninFunnel_signinButtonClicked_ignoredIfAlreadyClicked() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);
        assertTrue(mMediator.isContinueOrDismissClicked());
        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);

        verify(mSigninMetricsUtilsJniMock, times(1))
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.SIGNIN_BUTTON_CLICKED);
        verify(mDelegateMock, times(1)).acceptTermsOfService(true);
        verify(mDelegateMock, times(1))
                .recordUserSignInHistograms(
                        AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT);
    }

    @Test
    public void testSigninFunnel_signinDeclinedButtonClicked_ignoredIfAlreadyClicked() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);
        assertTrue(mMediator.isContinueOrDismissClicked());
        mModel.get(FullscreenSigninProperties.ON_DISMISS_CLICKED).onClick(null);

        verify(mSigninMetricsUtilsJniMock, never())
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE,
                        SigninFunnelEvent.SIGNIN_DECLINED_BUTTON_CLICKED);
        verify(mDelegateMock, never()).recordSigninDismissedHistograms();
        verify(mDelegateMock, times(1)).acceptTermsOfService(true);
        verify(mDelegateMock, never()).advanceToNextPage();
    }

    @Test
    public void testSigninFunnel_accountPickerShown() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        openAccountPicker();

        verify(mSigninMetricsUtilsJniMock)
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE,
                        SigninFunnelEvent.CHOOSE_OTHER_ACCOUNT_CLICKED);
        verify(mSigninMetricsUtilsJniMock)
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.ACCOUNT_PICKER_SHOWN);
    }

    @Test
    public void testSigninFunnel_defaultAccountSelected() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        createMediator();
        initializeNative();

        selectAccountInPicker(0);

        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT1);
        verify(mSigninMetricsUtilsJniMock)
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.DEFAULT_ACCOUNT_SELECTED);
    }

    @Test
    public void testSigninFunnel_nonDefaultAccountSelected() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        createMediator();
        initializeNative();

        selectAccountInPicker(1);

        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT2);
        verify(mSigninMetricsUtilsJniMock)
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE,
                        SigninFunnelEvent.NON_DEFAULT_ACCOUNT_SELECTED);
    }

    @Test
    public void testSigninFunnel_pendingSelectedAccount_noAccountSelectedEventLogged() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        mSelectedAccountEmail = TestAccounts.ACCOUNT2.getEmail();
        createMediator();
        initializeNative();

        assertSelectedAccountIsDisplayed(TestAccounts.ACCOUNT2);
        verify(mSigninMetricsUtilsJniMock, never())
                .logSigninFunnelEvent(anyInt(), eq(SigninFunnelEvent.DEFAULT_ACCOUNT_SELECTED));
        verify(mSigninMetricsUtilsJniMock, never())
                .logSigninFunnelEvent(anyInt(), eq(SigninFunnelEvent.NON_DEFAULT_ACCOUNT_SELECTED));
    }

    @Test
    public void testSigninFunnel_accountPickerDismissed() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        dismissAccountPicker(openAccountPicker());

        verify(mSigninMetricsUtilsJniMock)
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.ACCOUNT_PICKER_DISMISSED);
    }

    @Test
    public void testSigninFunnel_accountPickerDismissed_notLoggedAfterDestroy() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        PropertyModel dialogModel = openAccountPicker();
        destroyMediator();
        dismissAccountPicker(dialogModel);

        verify(mSigninMetricsUtilsJniMock, never())
                .logSigninFunnelEvent(anyInt(), eq(SigninFunnelEvent.ACCOUNT_PICKER_DISMISSED));
    }

    @Test
    public void testSigninFunnel_accountAdded() {
        setUpSigninSupported(true);
        createMediator();
        initializeNative();

        mMediator.onAccountAdded(TestAccounts.ACCOUNT1.getEmail());

        verify(mSigninMetricsUtilsJniMock)
                .logSigninFunnelEvent(
                        SigninAccessPoint.START_PAGE, SigninFunnelEvent.ACCOUNT_ADDED);
    }

    @Test
    public void testSigninFunnel_accountAdded_notLoggedAfterDestroy() {
        setUpSigninSupported(true);
        createMediator();
        initializeNative();

        FullscreenSigninCoordinator.Mediator mediator = mMediator;
        destroyMediator();
        mediator.onAccountAdded(TestAccounts.ACCOUNT1.getEmail());

        verify(mSigninMetricsUtilsJniMock, never())
                .logSigninFunnelEvent(anyInt(), eq(SigninFunnelEvent.ACCOUNT_ADDED));
    }

    @Test
    public void testSignInAbortedKeepsForcedSigninTitle() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        enableForcedSigninPolicy();
        when(mSigninManagerMock.getUserAcceptedAccountManagement()).thenReturn(true);
        FullscreenSigninCoordinator.disableAnimationsForTesting();

        createMediator();
        initializeNative();
        String forcedTitle =
                RuntimeEnvironment.getApplication()
                        .getString(R.string.signin_fre_title_signin_forced_by_policy);
        assertEquals(forcedTitle, mModel.get(FullscreenSigninProperties.TITLE_STRING));

        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);
        verify(mSigninManagerMock).signin(any(), anyInt(), mSignInCallbackCaptor.capture());
        // The title shows the welcome text while signing in.
        assertNotEquals(forcedTitle, mModel.get(FullscreenSigninProperties.TITLE_STRING));

        mSignInCallbackCaptor.getValue().onSignInAborted();

        assertEquals(forcedTitle, mModel.get(FullscreenSigninProperties.TITLE_STRING));
        assertFalse(mMediator.isContinueOrDismissClicked());
    }

    @Test
    public void testSyncDataManagedByPolicy_showsSubtitleWithoutSync() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mSyncServiceMock.isTypeManagedByPolicy(anyInt())).thenReturn(false);
        when(mSyncServiceMock.isTypeManagedByPolicy(UserSelectableType.LAST_TYPE)).thenReturn(true);

        createMediator();
        initializeNative();

        assertEquals(
                RuntimeEnvironment.getApplication()
                        .getString(R.string.signin_fre_subtitle_without_sync),
                mModel.get(FullscreenSigninProperties.SUBTITLE_STRING));
    }

    @Test
    public void testReset_afterSignInComplete_resetsStateAndAllowsContinueAgain() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mSigninManagerMock.getUserAcceptedAccountManagement()).thenReturn(true);
        FullscreenSigninCoordinator.disableAnimationsForTesting();
        createMediator();
        initializeNative();
        assertFalse(mMediator.isContinueOrDismissClicked());

        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);
        assertTrue(mModel.get(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT));
        assertTrue(mMediator.isContinueOrDismissClicked());
        verify(mSigninManagerMock).signin(any(), anyInt(), mSignInCallbackCaptor.capture());
        // In production, the account is primary once sign-in completes.
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        mSignInCallbackCaptor.getValue().onSignInComplete();
        verify(mDelegateMock).advanceToNextPage();
        assertTrue(mMediator.isContinueOrDismissClicked());

        mMediator.reset();

        assertFalse(mModel.get(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT));
        assertFalse(mModel.get(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER));
        assertFalse(mMediator.isContinueOrDismissClicked());
        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);
        // The selected account is already signed in, so the screen advances without signing in
        // again.
        verify(mDelegateMock, times(2)).advanceToNextPage();
        verify(mSigninManagerMock, times(1)).signin(any(), anyInt(), any());
    }

    @Test
    public void testDismissWhenSignedIn_signsOutThenAdvances() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        mModel.get(FullscreenSigninProperties.ON_DISMISS_CLICKED).onClick(null);

        assertTrue(mModel.get(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER));
        verify(mSigninManagerMock)
                .signOut(eq(SignoutReason.ABORT_SIGNIN), mSignOutCallbackCaptor.capture());
        verify(mDelegateMock).recordSigninDismissedHistograms();
        verify(mDelegateMock).acceptTermsOfService(true);
        verify(mDelegateMock, never()).advanceToNextPage();

        // In production, sign-out clears the primary account before calling back.
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(null);
        mSignOutCallbackCaptor.getValue().run();
        verify(mDelegateMock).advanceToNextPage();
    }

    @Test
    public void testDismissWhenSignedIn_ignoresClicksUntilReset() {
        setUpSigninSupported(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();
        assertFalse(mMediator.isContinueOrDismissClicked());

        mModel.get(FullscreenSigninProperties.ON_DISMISS_CLICKED).onClick(null);
        assertTrue(mMediator.isContinueOrDismissClicked());
        mModel.get(FullscreenSigninProperties.ON_DISMISS_CLICKED).onClick(null);
        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);

        verify(mSigninManagerMock, times(1))
                .signOut(eq(SignoutReason.ABORT_SIGNIN), mSignOutCallbackCaptor.capture());
        verify(mDelegateMock, times(1)).recordSigninDismissedHistograms();
        verify(mSigninManagerMock, never()).signin(any(), anyInt(), any());

        // Stays locked after sign-out completes, until the screen is reset. In production,
        // sign-out clears the primary account before calling back.
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(null);
        mSignOutCallbackCaptor.getValue().run();
        assertTrue(mMediator.isContinueOrDismissClicked());
        mMediator.reset();
        assertFalse(mModel.get(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER));
        assertFalse(mMediator.isContinueOrDismissClicked());
    }

    @Test
    public void testContinueOnAutomotive_displaysDeviceLockBeforeSignin() {
        setUpSigninSupported(true);
        DeviceInfo.setIsAutomotiveForTesting(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mSigninManagerMock.getUserAcceptedAccountManagement()).thenReturn(true);
        FullscreenSigninCoordinator.disableAnimationsForTesting();
        createMediator();
        initializeNative();

        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);
        verify(mDelegateMock).displayDeviceLockPage(TestAccounts.ACCOUNT1.getId());
        verify(mSigninManagerMock, never()).signin(any(), anyInt(), any());
        verify(mDelegateMock, never()).acceptTermsOfService(anyBoolean());
        verify(mDelegateMock, never()).recordUserSignInHistograms(anyInt());

        mMediator.proceedWithSignIn();
        verify(mDelegateMock).acceptTermsOfService(true);
        verify(mDelegateMock)
                .recordUserSignInHistograms(
                        AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT);
        verify(mSigninManagerMock)
                .signin(eq(TestAccounts.ACCOUNT1), eq(SigninAccessPoint.START_PAGE), any());
    }

    @Test
    public void testContinueOnAutomotive_deviceLockCanceled_allowsRetry() {
        setUpSigninSupported(true);
        DeviceInfo.setIsAutomotiveForTesting(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createMediator();
        initializeNative();

        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);
        // The legacy mediator doesn't lock the screen while device lock is shown, so the cancel
        // only has an effect in the state-machine mediator, which must accept Continue again.
        mMediator.onDeviceLockCanceled();

        assertFalse(mMediator.isContinueOrDismissClicked());
        mModel.get(FullscreenSigninProperties.ON_CONTINUE_AS_CLICKED).onClick(null);
        verify(mDelegateMock, times(2)).displayDeviceLockPage(TestAccounts.ACCOUNT1.getId());
    }

    @Test
    public void testSetConfig_updatesTitleAndSubtitleAfterInitialLoad() {
        setUpSigninSupported(true);
        createMediator();
        initializeNative();
        assertEquals("title", mModel.get(FullscreenSigninProperties.TITLE_STRING));
        assertEquals("subtitle", mModel.get(FullscreenSigninProperties.SUBTITLE_STRING));

        mMediator.setConfig(createConfig("new_title", "new_subtitle"));

        assertEquals("new_title", mModel.get(FullscreenSigninProperties.TITLE_STRING));
        assertEquals("new_subtitle", mModel.get(FullscreenSigninProperties.SUBTITLE_STRING));
    }

    @Test
    public void testSetConfig_preservesForcedByPolicyTitleAndSubtitle() {
        setUpSigninSupported(true);
        enableForcedSigninPolicy();
        createMediator();
        initializeNative();
        Context context = RuntimeEnvironment.getApplication();
        assertEquals(
                context.getString(R.string.signin_fre_title_signin_forced_by_policy),
                mModel.get(FullscreenSigninProperties.TITLE_STRING));
        assertEquals(
                context.getString(R.string.signin_fre_subtitle_signin_forced_by_policy),
                mModel.get(FullscreenSigninProperties.SUBTITLE_STRING));

        mMediator.setConfig(createConfig("new_title", "new_subtitle"));

        assertEquals(
                context.getString(R.string.signin_fre_title_signin_forced_by_policy),
                mModel.get(FullscreenSigninProperties.TITLE_STRING));
        assertEquals(
                context.getString(R.string.signin_fre_subtitle_signin_forced_by_policy),
                mModel.get(FullscreenSigninProperties.SUBTITLE_STRING));
    }

    @Test
    public void testSetConfig_preservesUnsupportedSigninTitleAndSubtitle() {
        setUpSigninSupported(false);
        createMediator();
        initializeNative();
        Context context = RuntimeEnvironment.getApplication();
        assertEquals(
                context.getString(R.string.fre_welcome),
                mModel.get(FullscreenSigninProperties.TITLE_STRING));
        assertNull(mModel.get(FullscreenSigninProperties.SUBTITLE_STRING));

        mMediator.setConfig(createConfig("new_title", "new_subtitle"));

        assertEquals(
                context.getString(R.string.fre_welcome),
                mModel.get(FullscreenSigninProperties.TITLE_STRING));
        assertNull(mModel.get(FullscreenSigninProperties.SUBTITLE_STRING));
    }

    private void setUpSigninSupported(boolean isSigninSupported) {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(isSigninSupported);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        if (!isSigninSupported) return;

        UserPrefs.setPrefServiceForTesting(mPrefServiceMock);
        lenient().when(mPrefServiceMock.getBoolean(Pref.SIGNIN_ALLOWED)).thenReturn(true);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
    }

    /**
     * Enables the forced sign-in policy and reports that policies are present. Must be called
     * before {@link #createMediator()}, because it replaces the policy load listener.
     */
    private void enableForcedSigninPolicy() {
        LocalStatePrefsJni.setInstanceForTesting(mLocalStatePrefsNativeMock);
        LocalStatePrefs.setNativePrefsLoadedForTesting(true);
        when(mLocalStatePrefsNativeMock.getPrefService()).thenReturn(mLocalPrefsServiceMock);
        when(mLocalPrefsServiceMock.getBoolean(Pref.FORCE_BROWSER_SIGNIN)).thenReturn(true);
        mPolicyLoadListener = new OneshotSupplierImpl<>();
        mPolicyLoadListener.set(true);
        when(mDelegateMock.getPolicyLoadListener()).thenReturn(mPolicyLoadListener);
    }

    private void createMediator() {
        mMediator =
                new FullscreenSigninMediator(
                        RuntimeEnvironment.getApplication(),
                        mModalDialogManagerMock,
                        mDelegateMock,
                        mPrivacyPreferencesManagerMock,
                        createConfig("title", "subtitle"),
                        SigninAccessPoint.START_PAGE);
        mModel = mMediator.getModel();
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private FullscreenSigninConfig createConfig(String title, String subtitle) {
        return new FullscreenSigninConfig(
                title,
                subtitle,
                /* dismissText= */ "dismiss",
                mLogoId,
                /* shouldDisableSignin= */ false,
                /* surveyType= */ null,
                mSelectedAccountEmail,
                SigninFlow.DEFAULT_SIGNIN);
    }

    private PropertyModel openAccountPicker() {
        mModel.get(FullscreenSigninProperties.ON_SELECTED_ACCOUNT_CLICKED).onClick(null);
        ArgumentCaptor<PropertyModel> dialogModelCaptor =
                ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManagerMock, atLeastOnce())
                .showDialog(dialogModelCaptor.capture(), anyInt());
        return dialogModelCaptor.getValue();
    }

    private void selectAccountInPicker(int accountIndex) {
        PropertyModel dialogModel = openAccountPicker();
        ViewGroup accountPickerView =
                (ViewGroup) dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        // RecyclerView only creates rows once it has been laid out, and the dialog isn't attached
        // to a window in this test, so lay it out manually.
        accountPickerView.measure(0, 0);
        accountPickerView.layout(0, 0, 100, 1000);
        accountPickerView.getChildAt(accountIndex).performClick();
    }

    private void dismissAccountPicker(PropertyModel dialogModel) {
        dialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onDismiss(dialogModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
    }

    private ChildAccountStatusListener captureChildAccountStatusListener() {
        ArgumentCaptor<ChildAccountStatusListener> listenerCaptor =
                ArgumentCaptor.forClass(ChildAccountStatusListener.class);
        verify(mFakeAccountManagerFacade, atLeastOnce())
                .checkIsSubjectToParentalControls(any(), listenerCaptor.capture());
        return listenerCaptor.getValue();
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
