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
import android.os.Build;

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
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowRoleManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoTriggerStateListener;
import org.chromium.chrome.browser.util.DefaultBrowserInfo;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultBrowserState;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultInfo;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit test for {@link DefaultBrowserPromoUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.Q)
public class DefaultBrowserPromoUtilsTest {
    @Mock private DefaultBrowserPromoImpressionCounter mCounter;
    @Mock private Tracker mMockTracker;
    @Mock private Profile mProfile;
    @Mock private ManagedMessageDispatcher mMockMessageDispatcher;
    @Mock private SearchEngineChoiceService mMockSearchEngineChoiceService;
    @Mock private InsetObserver mInsetObserver;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;

    private ShadowRoleManager mShadowRoleManager;

    DefaultBrowserPromoUtils mUtils;

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

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            mShadowRoleManager = shadowOf(mActivity.getSystemService(RoleManager.class));
            mShadowRoleManager.addAvailableRole(RoleManager.ROLE_BROWSER);
        }

        mUtils = new DefaultBrowserPromoUtils(mCounter);
        setDepsMockWithDefaultValues();

        DefaultBrowserInfo.setDefaultInfoForTests(
                createDefaultInfo(DefaultBrowserState.NO_DEFAULT));
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
        DefaultBrowserInfo.setDefaultInfoForTests(
                createDefaultInfo(DefaultBrowserState.NO_DEFAULT));
        Assert.assertTrue(
                "Should promo disambiguation sheet on Q.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    // --- Q above ---
    @Test
    public void testPromo_Q_No_Default() {
        DefaultBrowserInfo.setDefaultInfoForTests(
                createDefaultInfo(DefaultBrowserState.NO_DEFAULT));
        Assert.assertTrue(
                "Should promo role manager when there is no default browser on Q+.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testPromo_Q_Other_Default() {
        DefaultBrowserInfo.setDefaultInfoForTests(
                createDefaultInfo(DefaultBrowserState.OTHER_DEFAULT));
        Assert.assertTrue(
                "Should promo role manager when there is another default browser on Q+.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testPromo_Q_RoleHeld() {
        mShadowRoleManager.addHeldRole(RoleManager.ROLE_BROWSER);
        Assert.assertFalse(
                "Should Not show role manager promo when Role already held on Q+.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
    }

    @Test
    public void testPromo_Q_RoleNotAvailable() {
        mShadowRoleManager.removeAvailableRole(RoleManager.ROLE_BROWSER);
        Assert.assertFalse(
                "Should Not show role manager promo when Role is not available on Q+.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
    }

    // --- P below ---
    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testNoPromo_P() {
        Assert.assertFalse("Should not promo on P-.", mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertTrue(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    // --- prerequisites ---
    @Test
    public void testPromo_increasedPromoCount() {
        when(mCounter.getMaxPromoCount()).thenReturn(100);
        when(mCounter.getPromoCount()).thenReturn(99);
        Assert.assertTrue(
                "Should promo when promo count does not reach the upper limit.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testNoPromo_greaterThanMaxPromoCount() {
        when(mCounter.getPromoCount()).thenReturn(1);
        when(mCounter.getMaxPromoCount()).thenReturn(1);
        Assert.assertFalse(
                "Should not promo when promo count reaches the upper limit.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertTrue(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_DEFAULT_BROWSER_PROMO})
    public void testNoPromo_featureDisabled() {
        DefaultBrowserInfo.setDefaultInfoForTests(
                createDefaultInfo(DefaultBrowserState.NO_DEFAULT));
        Assert.assertFalse(
                "Should not promo when the feature is disabled.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertTrue(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testNoPromo_lessThanMinSessionCount() {
        when(mCounter.getSessionCount()).thenReturn(1);
        when(mCounter.getMinSessionCount()).thenReturn(3);
        Assert.assertFalse(
                "Should not promo when session count has not reached the required amount.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testNoPromo_isOtherChromeDefault() {
        DefaultBrowserInfo.setDefaultInfoForTests(
                createDefaultInfo(DefaultBrowserState.OTHER_CHROME_DEFAULT));
        Assert.assertFalse(
                "Should not promo when another chrome channel browser has been default.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testNoPromo_isCurrentChromeDefault() {
        DefaultBrowserInfo.setDefaultInfoForTests(
                createDefaultInfo(DefaultBrowserState.CHROME_DEFAULT));
        Assert.assertFalse(
                "Should not promo when chrome has been default.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
    }

    @Test
    public void testNoPromo_defaultBrowserInfoNotFetched() {
        DefaultBrowserInfo.setDefaultInfoForTests(null);
        Assert.assertFalse(
                "Should not promo when unable to fetch default info.",
                mUtils.shouldShowRoleManagerPromo(mActivity));
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
        Assert.assertTrue(mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertFalse(mUtils.shouldShowNonRoleManagerPromo(mActivity));
        mUtils.maybeShowDefaultBrowserPromoMessages(mActivity, mWindowAndroid, mProfile);
        verify(mMockMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
    public void testNoMessagePromo_featureEngagementBlocker() {
        mShadowRoleManager.removeAvailableRole(RoleManager.ROLE_BROWSER);
        when(mMockTracker.shouldTriggerHelpUi(any())).thenReturn(false);

        Assert.assertFalse(mUtils.shouldShowRoleManagerPromo(mActivity));
        Assert.assertTrue(mUtils.shouldShowNonRoleManagerPromo(mActivity));

        mUtils.maybeShowDefaultBrowserPromoMessages(mActivity, mWindowAndroid, mProfile);

        verify(mMockMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
    public void testShowMessagePromo() {
        mShadowRoleManager.removeAvailableRole(RoleManager.ROLE_BROWSER);
        when(mMockTracker.shouldTriggerHelpUi(any())).thenReturn(true);

        Assert.assertFalse(mUtils.shouldShowRoleManagerPromo(mActivity));
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
        DefaultBrowserPromoTriggerStateListener listener =
                Mockito.mock(DefaultBrowserPromoTriggerStateListener.class);
        mUtils.addListener(listener);
        mUtils.notifyDefaultBrowserPromoVisible();
        verify(listener).onDefaultBrowserPromoTriggered();

        mUtils.removeListener(listener);
        mUtils.notifyDefaultBrowserPromoVisible();
        verify(listener).onDefaultBrowserPromoTriggered();
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

        when(mProfile.isOffTheRecord()).thenReturn(false);
    }

    private DefaultInfo createDefaultInfo(@DefaultBrowserState int defaultState) {
        return new DefaultBrowserInfo.DefaultInfo(
                defaultState,
                /* isChromeSystem= */ true,
                /* isDefaultSystem= */ true,
                /* browserCount= */ 2,
                /* systemCount= */ 0,
                /* isChromePreStableInstalled */ false);
    }
}
