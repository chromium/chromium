// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeController.AutoDarkSettingsChangeSource;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings.AutoDarkSiteSettingObserver;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.url.GURL;

/** Unit tests for {@link WebContentsDarkModeController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {WebContentsDarkModeControllerUnitTest.ShadowApplicationStatus.class,
                ShadowRecordHistogram.class})
public class WebContentsDarkModeControllerUnitTest {
    @Implements(ApplicationStatus.class)
    static class ShadowApplicationStatus {
        @ApplicationState
        static int sApplicationState;
        @Nullable
        static ApplicationStateListener sLastListener;

        @Implementation
        public static int getStateForApplication() {
            return sApplicationState;
        }

        @Implementation
        public static void registerApplicationStateListener(ApplicationStateListener listener) {
            sLastListener = listener;
        }
    }

    static class TestNightModeStateProvider implements NightModeStateProvider {
        public boolean mIsInNightMode;
        public @Nullable Observer mObserver;

        @Override
        public boolean isInNightMode() {
            return mIsInNightMode;
        }

        @Override
        public void addObserver(@NonNull Observer observer) {
            mObserver = observer;
        }

        @Override
        public void removeObserver(@NonNull Observer observer) {
            if (observer == mObserver) mObserver = null;
        }
    }

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    UserPrefs.Natives mMockUserPrefJni;
    @Mock
    WebsitePreferenceBridge.Natives mMockWebsitePreferenceBridgeJni;
    @Mock
    PrefService mMockPrefService;
    @Mock
    Profile mMockProfile;
    @Mock
    GURL mMockGurl;

    TestNightModeStateProvider mNightModeStateProvider;
    WebContentsDarkModeController mWebContentsDarkModeController;
    boolean mIsGlobalSettingsEnabled;
    @ContentSettingValues
    int mIsAutoDarkEnabledForUrlContentSettingValue;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefJni);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mMockWebsitePreferenceBridgeJni);
        Mockito.when(mMockUserPrefJni.get(any())).thenReturn(mMockPrefService);

        mNightModeStateProvider = new TestNightModeStateProvider();
        GlobalNightModeStateProviderHolder.setInstanceForTesting(mNightModeStateProvider);

        Profile.setLastUsedProfileForTesting(mMockProfile);
        ShadowApplicationStatus.sApplicationState = ApplicationState.HAS_RUNNING_ACTIVITIES;

        ShadowRecordHistogram.reset();

        Mockito.doAnswer(invocation -> {
                   mIsGlobalSettingsEnabled = (boolean) invocation.getArguments()[2];
                   return null;
               })
                .when(mMockWebsitePreferenceBridgeJni)
                .setContentSettingEnabled(eq(mMockProfile),
                        eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT), anyBoolean());
        Mockito.doAnswer(invocation -> mIsGlobalSettingsEnabled)
                .when(mMockWebsitePreferenceBridgeJni)
                .isContentSettingEnabled(
                        eq(mMockProfile), eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT));
        Mockito.doAnswer(invocation -> {
                   mIsAutoDarkEnabledForUrlContentSettingValue = (int) invocation.getArguments()[4];
                   return null;
               })
                .when(mMockWebsitePreferenceBridgeJni)
                .setContentSettingDefaultScope(eq(mMockProfile),
                        eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT), notNull(), notNull(),
                        anyInt());
        Mockito.doAnswer(invocation -> mIsAutoDarkEnabledForUrlContentSettingValue)
                .when(mMockWebsitePreferenceBridgeJni)
                .getContentSetting(eq(mMockProfile), eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT),
                        notNull(), notNull());
    }

    @After
    public void tearDown() {
        GlobalNightModeStateProviderHolder.setInstanceForTesting(null);
        WebContentsDarkModeController.setTestInstance(null);
        Profile.setLastUsedProfileForTesting(null);

        ShadowApplicationStatus.sApplicationState = ApplicationState.UNKNOWN;
        ShadowApplicationStatus.sLastListener = null;
        ShadowRecordHistogram.reset();
    }

    @Test
    public void testApplicationStatus_RunningActivities() {
        mWebContentsDarkModeController = WebContentsDarkModeController.createInstance();

        assertIsObservingNightMode(true);
        Assert.assertEquals("Controller should be started and observing application status.",
                mWebContentsDarkModeController, ShadowApplicationStatus.sLastListener);

        ShadowApplicationStatus.sApplicationState = ApplicationState.HAS_STOPPED_ACTIVITIES;
        ShadowApplicationStatus.sLastListener.onApplicationStateChange(
                ShadowApplicationStatus.sApplicationState);
        assertIsObservingNightMode(false);
    }

    @Test
    public void testApplicationStatus_NoRunningActivities() {
        ShadowApplicationStatus.sApplicationState = ApplicationState.HAS_STOPPED_ACTIVITIES;

        mWebContentsDarkModeController = WebContentsDarkModeController.createInstance();
        Assert.assertEquals("Controller should be started and observing application status.",
                mWebContentsDarkModeController, ShadowApplicationStatus.sLastListener);
        assertIsObservingNightMode(false);
    }

    private void doTestSetAutoDarkGlobalSettingsEnabled(boolean enabled) {
        mNightModeStateProvider.mIsInNightMode = true;

        WebContentsDarkModeController.setGlobalUserSettings(enabled);
        Assert.assertEquals(
                "Auto dark settings state incorrect.", enabled, mIsGlobalSettingsEnabled);
        assertForceDarkModeEnabled(enabled);
        assertAutoDarkModeChangeSourceRecorded(
                AutoDarkSettingsChangeSource.THEME_SETTINGS, enabled, 1);
    }

    @Test
    public void testGlobalSettingsEnabled() {
        doTestSetAutoDarkGlobalSettingsEnabled(true);
    }

    @Test
    public void testGlobalSettingsDisabled() {
        doTestSetAutoDarkGlobalSettingsEnabled(false);
    }

    private void doTestSetAutoDarkForUrl(boolean enableForUrl) {
        mNightModeStateProvider.mIsInNightMode = true;
        Mockito.doReturn(ContentSettingValues.ALLOW)
                .when(mMockWebsitePreferenceBridgeJni)
                .getDefaultContentSetting(
                        eq(mMockProfile), eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT));

        WebContentsDarkModeController.setEnabledForUrl(mMockProfile, mMockGurl, enableForUrl);

        Assert.assertEquals("Auto dark for URL is incorrect.", enableForUrl,
                WebContentsDarkModeController.isEnabledForUrl(mMockProfile, mMockGurl));
        assertAutoDarkModeChangeSourceRecorded(
                AutoDarkSettingsChangeSource.APP_MENU, enableForUrl, 1);
    }

    @Test
    public void testEnableForUrl_Enabled() {
        doTestSetAutoDarkForUrl(true);
    }

    @Test
    public void testEnableForUrl_Disabled() {
        doTestSetAutoDarkForUrl(false);
    }

    @Test
    public void testAutoDarkSiteSettingsObserver_DefaultValueChanged() {
        AutoDarkSiteSettingObserver observer = WebContentsDarkModeController.createInstance();

        observer.onDefaultValueChanged(true);
        assertAutoDarkModeChangeSourceRecorded(
                AutoDarkSettingsChangeSource.SITE_SETTINGS_GLOBAL, true, 1);
        observer.onDefaultValueChanged(false);
        assertAutoDarkModeChangeSourceRecorded(
                AutoDarkSettingsChangeSource.SITE_SETTINGS_GLOBAL, false, 1);
    }

    @Test
    public void testAutoDarkSiteSettingsObserver_SiteExceptionChanged() {
        AutoDarkSiteSettingObserver observer = WebContentsDarkModeController.createInstance();
        observer.onSiteExceptionChanged(true);
        assertAutoDarkModeChangeSourceRecorded(
                AutoDarkSettingsChangeSource.SITE_SETTINGS_EXCEPTION_LIST, false, 1);
        observer.onSiteExceptionChanged(false);
        assertAutoDarkModeChangeSourceRecorded(
                AutoDarkSettingsChangeSource.SITE_SETTINGS_EXCEPTION_LIST, true, 1);
    }

    @Test
    public void testOnNightModeChange() {
        mIsGlobalSettingsEnabled = true;
        mNightModeStateProvider.mIsInNightMode = false;

        mWebContentsDarkModeController = WebContentsDarkModeController.createInstance();
        assertIsObservingNightMode(true);
        assertForceDarkModeEnabled(false);

        mNightModeStateProvider.mIsInNightMode = true;
        mNightModeStateProvider.mObserver.onNightModeStateChanged();
        assertForceDarkModeEnabled(true);
    }

    private void assertForceDarkModeEnabled(boolean enabled) {
        Mockito.verify(mMockPrefService)
                .setBoolean(eq(Pref.WEB_KIT_FORCE_DARK_MODE_ENABLED), eq(enabled));
    }

    private void assertIsObservingNightMode(boolean isObserving) {
        if (isObserving) {
            Assert.assertNotNull("Controller should be started and observing night mode.",
                    mNightModeStateProvider.mObserver);
        } else {
            Assert.assertNull("Controller will not should not observing night mode.",
                    mNightModeStateProvider.mObserver);
        }
    }

    private void assertAutoDarkModeChangeSourceRecorded(
            @AutoDarkSettingsChangeSource int source, boolean enabled, int expectedCounts) {
        String histogramName = "Android.DarkTheme.AutoDarkMode.SettingsChangeSource."
                + (enabled ? "Enabled" : "Disabled");
        int actualCount = RecordHistogram.getHistogramValueCountForTesting(histogramName, source);
        Assert.assertEquals("Histogram <" + histogramName + "> for sample <" + source
                        + "> is not recorded correctly.",
                expectedCounts, actualCount);
    }
}
