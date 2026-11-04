// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
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
import org.robolectric.Robolectric;

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SigninFunnelEvent;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link FullscreenSigninAndHistorySyncCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenSigninAndHistorySyncCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private WindowAndroid mWindowAndroidMock;
    @Mock private ModalDialogManager mModalDialogManagerMock;
    @Mock private PrivacyPreferencesManager mPrivacyPreferencesManagerMock;
    @Mock private FullscreenSigninAndHistorySyncCoordinator.Delegate mDelegateMock;
    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncherMock;
    @Mock private ProfileProvider mProfileProviderMock;
    @Mock private Profile mProfileMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;
    @Mock private Tracker mTrackerMock;
    @Mock private AccountPreviewDataService mAccountPreviewDataServiceMock;
    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private SyncService mSyncServiceMock;

    private final OneshotSupplierImpl<ProfileProvider> mProfileSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<Boolean> mPolicyLoadListener = new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<Boolean> mChildAccountStatusSupplier =
            new OneshotSupplierImpl<>();
    private final Promise<Void> mNativeInitializationPromise = new Promise<>();

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsJniMock);
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManagerMock);
        TrackerFactory.setTrackerForTests(mTrackerMock);

        when(mProfileProviderMock.getOriginalProfile()).thenReturn(mProfileMock);
        mProfileSupplier.set(mProfileProviderMock);

        when(mDelegateMock.getPolicyLoadListener()).thenReturn(mPolicyLoadListener);
        when(mDelegateMock.getChildAccountStatusSupplier()).thenReturn(mChildAccountStatusSupplier);
        when(mDelegateMock.getNativeInitializationPromise())
                .thenReturn(mNativeInitializationPromise);
    }

    @Test
    public void testDestroy_whenShouldCallTrackerDismissed_callsTrackerDismissed() {
        FullscreenSigninAndHistorySyncCoordinator coordinator =
                createCoordinator(/* shouldCallTrackerDismissed= */ true);

        coordinator.destroy();

        verify(mTrackerMock).dismissed(FeatureConstants.FULLSCREEN_SIGNIN_PROMO_FEATURE);
    }

    @Test
    public void testDestroy_whenShouldNotCallTrackerDismissed_doesNotCallTrackerDismissed() {
        FullscreenSigninAndHistorySyncCoordinator coordinator =
                createCoordinator(/* shouldCallTrackerDismissed= */ false);

        coordinator.destroy();

        verify(mTrackerMock, never()).dismissed(FeatureConstants.FULLSCREEN_SIGNIN_PROMO_FEATURE);
    }

    @Test
    public void testDisplayDeviceLockPage_resultCanceled_keepsSigninScreen() {
        FullscreenSigninAndHistorySyncCoordinator coordinator =
                createCoordinator(/* shouldCallTrackerDismissed= */ false);
        // The sign-in screen must be showing, so the result reaches a sign-in coordinator.
        assertNotNull(coordinator.getView().findViewById(R.id.fullscreen_signin));
        CoreAccountId accountId = new CoreAccountId(new GaiaId("gaia-id"));
        coordinator.displayDeviceLockPage(accountId);
        ArgumentCaptor<WindowAndroid.IntentCallback> callbackCaptor =
                ArgumentCaptor.forClass(WindowAndroid.IntentCallback.class);
        verify(mDeviceLockActivityLauncherMock)
                .launchDeviceLockActivity(
                        eq(mActivity),
                        eq(accountId),
                        eq(true),
                        eq(mWindowAndroidMock),
                        callbackCaptor.capture(),
                        eq(DeviceLockActivityLauncher.Source.FULLSCREEN_SIGNIN));

        callbackCaptor.getValue().onIntentCompleted(Activity.RESULT_CANCELED, null);

        verify(mDelegateMock, never()).onFlowComplete(any());
        assertNotNull(coordinator.getView().findViewById(R.id.fullscreen_signin));
    }

    @Test
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void testOnConfigurationChange_doesNotDuplicateMetrics() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        UserPrefs.setPrefServiceForTesting(mPrefServiceMock);
        when(mPrefServiceMock.getBoolean(Pref.SIGNIN_ALLOWED)).thenReturn(true);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
        IdentityServicesProvider.setAccountPreviewDataServiceForTesting(
                mAccountPreviewDataServiceMock);

        mPolicyLoadListener.set(false);
        mChildAccountStatusSupplier.set(false);
        mNativeInitializationPromise.fulfill(null);

        HistogramWatcher signinStartedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Started", SigninAccessPoint.FULLSCREEN_SIGNIN_PROMO);

        FullscreenSigninAndHistorySyncCoordinator coordinator =
                createCoordinator(/* shouldCallTrackerDismissed= */ false);
        RobolectricUtil.runAllBackgroundAndUi();

        signinStartedWatcher.assertExpected();
        verify(mSigninMetricsUtilsJniMock, times(1))
                .logSigninFunnelEvent(
                        SigninAccessPoint.FULLSCREEN_SIGNIN_PROMO,
                        SigninFunnelEvent.PROMO_SHOWN_NO_ACCOUNT);

        coordinator.onConfigurationChange();
        RobolectricUtil.runAllBackgroundAndUi();

        signinStartedWatcher.assertExpected();
        // Verify promo shown was not recorded again.
        verify(mSigninMetricsUtilsJniMock, times(1))
                .logSigninFunnelEvent(
                        SigninAccessPoint.FULLSCREEN_SIGNIN_PROMO,
                        SigninFunnelEvent.PROMO_SHOWN_NO_ACCOUNT);
    }

    private FullscreenSigninAndHistorySyncCoordinator createCoordinator(
            boolean shouldCallTrackerDismissed) {
        FullscreenSigninAndHistorySyncConfig config =
                FullscreenSigninAndHistorySyncConfig.builder(
                                "title", "subtitle", "dismiss", "historyTitle", "historySubtitle")
                        .shouldCallTrackerDismissed(shouldCallTrackerDismissed)
                        .build();
        return new FullscreenSigninAndHistorySyncCoordinator(
                mWindowAndroidMock,
                mActivity,
                mModalDialogManagerMock,
                mProfileSupplier,
                mPrivacyPreferencesManagerMock,
                config,
                SigninAccessPoint.FULLSCREEN_SIGNIN_PROMO,
                mDelegateMock,
                /* activityStartTime= */ 0L,
                mDeviceLockActivityLauncherMock);
    }
}
