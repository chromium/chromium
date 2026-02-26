// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.provider.Settings;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowRoleManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoTriggerStateListener;
import org.chromium.chrome.browser.util.DefaultBrowserInfo;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultBrowserState;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.time.Duration;
import java.util.Arrays;
import java.util.Collection;

/** Unit test for {@link DefaultBrowserPromoUtils}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class DefaultBrowserPromoUtilsTest {
    @Mock private DefaultBrowserPromoImpressionCounter mCounter;
    @Mock private DefaultBrowserStateProvider mProvider;
    @Mock private Tracker mMockTracker;
    @Mock private Profile mProfile;
    @Mock private ManagedMessageDispatcher mMockMessageDispatcher;
    @Mock private SearchEngineChoiceService mMockSearchEngineChoiceService;
    @Mock private InsetObserver mInsetObserver;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;

    private ShadowRoleManager mShadowRoleManager;

    DefaultBrowserPromoUtils mUtils;

    @Parameter(0)
    public boolean mFlagEnabled;

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    private static class TestingDefaultBrowserPromoUtils extends DefaultBrowserPromoUtils {

        private final DefaultBrowserInfo.DefaultInfo mTestInfo;

        TestingDefaultBrowserPromoUtils(
                DefaultBrowserPromoImpressionCounter impressionCounter,
                DefaultBrowserStateProvider stateProvider,
                DefaultBrowserInfo.DefaultInfo testInfo) {
            super(impressionCounter, stateProvider);
            mTestInfo = testInfo;
        }

        @Override
        protected void fetchDefaultBrowserInfo(
                org.chromium.base.Callback<DefaultBrowserInfo.@Nullable DefaultInfo> callback) {
            // This approach is used so that the lambda code is returned immediately.
            callback.onResult(mTestInfo);
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mWindowAndroid =
                new ActivityWindowAndroid(
                        mActivity,
                        false,
                        IntentRequestTracker.createFromActivity(mActivity),
                        mInsetObserver,
                        /* trackOcclusion= */ true);
        TrackerFactory.setTrackerForTests(mMockTracker);
        MessagesFactory.attachMessageDispatcher(mWindowAndroid, mMockMessageDispatcher);
        SearchEngineChoiceService.setInstanceForTests(mMockSearchEngineChoiceService);

        mShadowRoleManager = shadowOf(mActivity.getSystemService(RoleManager.class));
        mShadowRoleManager.addAvailableRole(RoleManager.ROLE_BROWSER);

        mUtils = new DefaultBrowserPromoUtils(mCounter, mProvider);
        setDepsMockWithDefaultValues();
        FeatureOverrides.newBuilder()
                .flag(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ENTRY_POINT, mFlagEnabled)
                .apply();
    }

    @After
    public void tearDown() {
        MessagesFactory.detachMessageDispatcher(mMockMessageDispatcher);
        TrackerFactory.setTrackerForTests(null);
        mActivity.finish();
        mWindowAndroid.destroy();
    }

    @Test
    public void testBasicPromo() {
        Assert.assertTrue(
                "Should promo disambiguation sheet on Q.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    // --- Q above ---
    @Test
    public void testPromo_Q_No_Default() {
        Assert.assertTrue(
                "Should promo role manager when there is no default browser on Q+.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testPromo_Q_Other_Default() {
        when(mProvider.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(createResolveInfo("android", 1));
        Assert.assertTrue(
                "Should promo role manager when there is another default browser on Q+.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testPromo_Q_RoleHeld() {
        mShadowRoleManager.addHeldRole(RoleManager.ROLE_BROWSER);
        Assert.assertFalse(
                "Should Not show role manager promo when Role already held on Q+.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertTrue(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testPromo_Q_RoleNotAvailable() {
        mShadowRoleManager.removeAvailableRole(RoleManager.ROLE_BROWSER);
        Assert.assertFalse(
                "Should Not show role manager promo when Role is not available on Q+.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertTrue(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    // --- prerequisites ---
    @Test
    public void testPromo_increasedPromoCount() {
        when(mCounter.getMaxPromoCount()).thenReturn(100);
        when(mCounter.getPromoCount()).thenReturn(99);
        Assert.assertTrue(
                "Should promo when promo count does not reach the upper limit.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testNoPromo_greaterThanMaxPromoCount() {
        when(mCounter.getPromoCount()).thenReturn(1);
        when(mCounter.getMaxPromoCount()).thenReturn(1);
        Assert.assertFalse(
                "Should not promo when promo count reaches the upper limit.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertTrue(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_DEFAULT_BROWSER_PROMO})
    public void testNoPromo_featureDisabled() {
        Assert.assertFalse(
                "Should not promo when the feature is disabled.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertTrue(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testNoPromo_lessThanMinSessionCount() {
        when(mCounter.getSessionCount()).thenReturn(1);
        when(mCounter.getMinSessionCount()).thenReturn(3);
        Assert.assertFalse(
                "Should not promo when session count has not reached the required amount.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testNoPromo_isOtherChromeDefault() {
        when(mProvider.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(
                        createResolveInfo(
                                DefaultBrowserStateProvider.CHROME_STABLE_PACKAGE_NAME, 1));
        Assert.assertFalse(
                "Should not promo when another chrome channel browser has been default.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testNoPromo_isCurrentChromeDefault() {
        when(mProvider.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(
                        createResolveInfo(
                                ContextUtils.getApplicationContext().getPackageName(), 1));
        Assert.assertFalse(
                "Should not promo when chrome has been default.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testNoPromo_isNoDefaultWithPreChromePreStableInstalled() {
        when(mProvider.getDefaultWebBrowserActivityResolveInfo()).thenReturn(null);
        when(mProvider.isChromeStable()).thenReturn(true);
        when(mProvider.isChromePreStableInstalled()).thenReturn(true);

        Assert.assertFalse(
                "Should not promo when current is chrome stable and has chrome pre stable"
                        + " installed.",
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
    public void testNoMessagePromo_featureDisabled() {
        mUtils.maybeShowDefaultBrowserPromoMessages(mActivity, mWindowAndroid, mProfile);
        verify(mMockMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
    public void testNoMessagePromo_offTheRecordProfile() {
        // No Profile.
        mUtils.maybeShowDefaultBrowserPromoMessages(mActivity, mWindowAndroid, null);
        verify(mMockMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());

        // Incognito profile
        when(mProfile.isOffTheRecord()).thenReturn(true);
        mUtils.maybeShowDefaultBrowserPromoMessages(mActivity, mWindowAndroid, mProfile);
        verify(mMockMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
    public void testNoMessagePromo_shouldShowRoleManagerPromo() {
        Assert.assertTrue(
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
        mUtils.maybeShowDefaultBrowserPromoMessages(mActivity, mWindowAndroid, mProfile);
        verify(mMockMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
    public void testNoMessagePromo_featureEngagementBlocker() {
        mShadowRoleManager.removeAvailableRole(RoleManager.ROLE_BROWSER);
        when(mMockTracker.shouldTriggerHelpUi(any())).thenReturn(false);

        Assert.assertFalse(
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertTrue(mUtils.shouldShowNonRoleManagerPromo(mActivity));

        mUtils.maybeShowDefaultBrowserPromoMessages(mActivity, mWindowAndroid, mProfile);

        verify(mMockMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
    public void testShowMessagePromo() {
        mShadowRoleManager.removeAvailableRole(RoleManager.ROLE_BROWSER);
        when(mMockTracker.shouldTriggerHelpUi(any())).thenReturn(true);

        Assert.assertFalse(
                mUtils.shouldShowRoleManagerPromo(
                        mActivity,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.CHROME_STARTUP));
        Assert.assertTrue(mUtils.shouldShowNonRoleManagerPromo(mActivity));

        mUtils.maybeShowDefaultBrowserPromoMessages(mActivity, mWindowAndroid, mProfile);

        ArgumentCaptor<PropertyModel> message = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMockMessageDispatcher).enqueueWindowScopedMessage(message.capture(), eq(false));
        Assert.assertEquals(
                "Message identifier should match.",
                MessageIdentifier.DEFAULT_BROWSER_PROMO,
                message.getValue().get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals(
                "Message title should match.",
                mActivity.getResources().getString(R.string.default_browser_promo_message_title),
                message.getValue().get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                "Message primary button text should match.",
                mActivity
                        .getResources()
                        .getString(R.string.default_browser_promo_message_settings_button),
                message.getValue().get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertEquals(
                "Message icon resource ID should match.",
                R.drawable.ic_chrome,
                message.getValue().get(MessageBannerProperties.ICON_RESOURCE_ID));
    }

    @Test
    public void testNotifyDefaultBrowserPromoVisible() {
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.EDUCATIONAL_TIP_LAST_DEFAULT_BROWSER_PROMO_TIMESTAMP);
        Assert.assertFalse(
                "Promo shouldn't have been shown recently.",
                DefaultBrowserPromoUtils.hasPromoShownRecently());

        DefaultBrowserPromoTriggerStateListener listener =
                Mockito.mock(DefaultBrowserPromoTriggerStateListener.class);
        mUtils.addListener(listener);
        mUtils.notifyDefaultBrowserPromoVisible();
        verify(listener).onDefaultBrowserPromoTriggered();

        mFakeTimeTestRule.advanceMillis(Duration.ofDays(1).toMillis());
        Assert.assertTrue(
                "Promo should still be considered recently shown.",
                DefaultBrowserPromoUtils.hasPromoShownRecently());
        mFakeTimeTestRule.advanceMillis(Duration.ofDays(8).toMillis());
        Assert.assertFalse(
                "Promo should no longer be considered recently shown.",
                DefaultBrowserPromoUtils.hasPromoShownRecently());

        mUtils.removeListener(listener);
        mUtils.notifyDefaultBrowserPromoVisible();
        verify(listener).onDefaultBrowserPromoTriggered();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ENTRY_POINT + ":show_app_menu_item/true"
    })
    public void testOnAppMenuItemClick_ShowRoleManager() {
        // Promo (Role Manager Dialog) has never been shown. mShadowRoleManger is set up so that the
        // role is available but not held (Chrome is not set to default).
        when(mCounter.getPromoCount()).thenReturn(0);

        when(mProvider.getCurrentDefaultBrowserState()).thenReturn(DefaultBrowserState.NO_DEFAULT);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DefaultBrowserPromo.EntryPoint.AppMenu",
                                DefaultBrowserState.NO_DEFAULT)
                        .build();

        DefaultBrowserInfo.DefaultInfo info =
                new DefaultBrowserInfo.DefaultInfo(
                        /* defaultBrowserState= */ DefaultBrowserState.NO_DEFAULT,
                        /* isChromeSystem= */ false,
                        /* isDefaultSystem= */ false,
                        /* browserCount= */ 1,
                        /* systemCount= */ 0,
                        /* isChromePreStableInstalled= */ false);

        reCreateUtilsWithTestInfo(info);

        mUtils.onMenuItemClick(
                mActivity,
                mWindowAndroid,
                DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.APP_MENU);

        watcher.assertExpected();

        // Verify we incremented the counter.
        verify(mCounter).onPromoShown();

        // Get the last Intent this activity tried to launch.
        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        Assert.assertNotNull("Should have launched an Intent", intent);

        // Check the "Action" of the Role Manager Dialog.
        Assert.assertEquals(
                "Should launch Role Manager Intent",
                "android.app.role.action.REQUEST_ROLE",
                intent.getAction());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ENTRY_POINT + ":show_app_menu_item/true"
    })
    public void testOnMenuItemClick_FallbackToSettings_RoleHeld() {
        // Promo (Role Manager Dialog) has never been shown.
        when(mCounter.getPromoCount()).thenReturn(0);
        // Chrome is already the default browser (Role is Held).
        mShadowRoleManager.addHeldRole(RoleManager.ROLE_BROWSER);

        DefaultBrowserInfo.DefaultInfo info =
                new DefaultBrowserInfo.DefaultInfo(
                        /* defaultBrowserState= */ DefaultBrowserState.CHROME_DEFAULT,
                        /* isChromeSystem= */ true,
                        /* isDefaultSystem= */ true,
                        /* browserCount= */ 0,
                        /* systemCount= */ 1,
                        /* isChromePreStableInstalled= */ false);

        reCreateUtilsWithTestInfo(info);

        mUtils.onMenuItemClick(
                mActivity,
                mWindowAndroid,
                DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.APP_MENU);

        // Should not increment counter since we skipped Role Manager.
        verify(mCounter, never()).onPromoShown();
        verifyOSSettingsFallbackIntentLaunched();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ENTRY_POINT + ":show_app_menu_item/true"
    })
    public void testOnMenuItemClick_FallbackToSettings_PromoShownBefore() {
        // Promo Count > 0 (Already shown once). Chrome is not set to default.
        when(mCounter.getPromoCount()).thenReturn(1);

        DefaultBrowserInfo.DefaultInfo info =
                new DefaultBrowserInfo.DefaultInfo(
                        /* defaultBrowserState= */ DefaultBrowserState.NO_DEFAULT,
                        /* isChromeSystem= */ false,
                        /* isDefaultSystem= */ false,
                        /* browserCount= */ 1,
                        /* systemCount= */ 0,
                        /* isChromePreStableInstalled= */ false);

        reCreateUtilsWithTestInfo(info);

        mUtils.onMenuItemClick(
                mActivity,
                mWindowAndroid,
                DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.APP_MENU);

        // Should not increment counter again.
        verify(mCounter, never()).onPromoShown();
        verifyOSSettingsFallbackIntentLaunched();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ENTRY_POINT + ":show_app_menu_item/true"
    })
    public void testOnMenuItemClick_FallbackToSettings_NullWindow() {
        // Promo never shown. Chrome is not set to default.
        when(mCounter.getPromoCount()).thenReturn(0);

        when(mProvider.getCurrentDefaultBrowserState()).thenReturn(DefaultBrowserState.NO_DEFAULT);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DefaultBrowserPromo.EntryPoint.Settings",
                                DefaultBrowserState.NO_DEFAULT)
                        .build();

        DefaultBrowserInfo.DefaultInfo info =
                new DefaultBrowserInfo.DefaultInfo(
                        /* defaultBrowserState= */ DefaultBrowserState.NO_DEFAULT,
                        /* isChromeSystem= */ false,
                        /* isDefaultSystem= */ false,
                        /* browserCount= */ 1,
                        /* systemCount= */ 0,
                        /* isChromePreStableInstalled= */ false);

        reCreateUtilsWithTestInfo(info);

        // Pass null for WindowAndroid. This would happen when the menu item in Settings (not App
        // Menu) is clicked.
        mUtils.onMenuItemClick(
                mActivity, null, DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.SETTINGS);

        // Should not increment counter since we are not showing the RoleManaerDialog.
        verify(mCounter, never()).onPromoShown();
        verifyOSSettingsFallbackIntentLaunched();
    }

    @Test
    public void testPrepareLaunchPromoIfNeeded_SetUpList() {
        // First click: Promo (Role Manager Dialog) has never been shown.
        when(mCounter.getPromoCount()).thenReturn(0);
        Assert.assertTrue(
                "Should show role manager on first click from Setup List.",
                mUtils.prepareLaunchPromoIfNeeded(
                        mActivity,
                        mWindowAndroid,
                        mMockTracker,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.SET_UP_LIST));

        // Second click: Promo Count > 0.
        when(mCounter.getPromoCount()).thenReturn(1);
        Assert.assertFalse(
                "Should NOT show role manager on subsequent clicks from Setup List.",
                mUtils.prepareLaunchPromoIfNeeded(
                        mActivity,
                        mWindowAndroid,
                        mMockTracker,
                        DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint.SET_UP_LIST));
    }

    private void verifyOSSettingsFallbackIntentLaunched() {
        // Should fallback to System Settings Intent.
        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        Assert.assertNotNull("Should have launched an Intent", intent);
        Assert.assertEquals(
                "Should launch System Settings",
                Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS,
                intent.getAction());
    }

    private void reCreateUtilsWithTestInfo(DefaultBrowserInfo.DefaultInfo info) {
        mUtils = new TestingDefaultBrowserPromoUtils(mCounter, mProvider, info);
    }

    private void setDepsMockWithDefaultValues() {
        when(mMockSearchEngineChoiceService.isDefaultBrowserPromoSuppressed()).thenReturn(false);

        when(mCounter.shouldShowPromo(anyBoolean())).thenCallRealMethod();
        when(mCounter.getMinSessionCount()).thenReturn(3);
        when(mCounter.getSessionCount()).thenReturn(10);
        when(mCounter.getPromoCount()).thenReturn(0);
        when(mCounter.getMaxPromoCount()).thenReturn(1);
        when(mCounter.getLastPromoInterval()).thenReturn(1000);
        when(mCounter.getMinPromoInterval()).thenReturn(10);

        when(mProvider.shouldShowPromo()).thenCallRealMethod();
        when(mProvider.isChromeStable()).thenReturn(false);
        when(mProvider.isChromePreStableInstalled()).thenReturn(false);
        // No Default
        when(mProvider.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(createResolveInfo("android", 0));
        when(mProvider.getCurrentDefaultBrowserState()).thenCallRealMethod();
        when(mProvider.getCurrentDefaultBrowserState(anyBoolean())).thenCallRealMethod();
        when(mProvider.getCurrentDefaultBrowserState(any(), anyBoolean())).thenCallRealMethod();

        when(mProfile.isOffTheRecord()).thenReturn(false);
    }

    private ResolveInfo createResolveInfo(String packageName, int match) {
        ResolveInfo resolveInfo = new ResolveInfo();
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        resolveInfo.activityInfo = activityInfo;
        resolveInfo.match = match;
        return resolveInfo;
    }
}
