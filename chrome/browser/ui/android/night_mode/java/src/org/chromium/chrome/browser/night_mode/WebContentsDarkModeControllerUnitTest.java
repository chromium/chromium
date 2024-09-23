// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;

import android.content.Context;

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

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.site_settings.AutoDarkMetrics.AutoDarkSettingsChangeSource;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.shadows.ShadowColorUtils;
import org.chromium.url.GURL;

/** Unit tests for {@link WebContentsDarkModeController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowColorUtils.class})
@SuppressWarnings("DoNotMock") // Mocking GURL
public class WebContentsDarkModeControllerUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock WebsitePreferenceBridge.Natives mMockWebsitePreferenceBridgeJni;
    @Mock Profile mMockProfile;
    @Mock GURL mMockGurl;
    @Mock Context mMockContext;

    boolean mIsGlobalSettingsEnabled;
    @ContentSettingValues int mIsAutoDarkEnabledForUrlContentSettingValue;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mMockWebsitePreferenceBridgeJni);

        ProfileManager.setLastUsedProfileForTesting(mMockProfile);

        Mockito.doAnswer(
                        invocation -> {
                            mIsGlobalSettingsEnabled = (boolean) invocation.getArguments()[2];
                            return null;
                        })
                .when(mMockWebsitePreferenceBridgeJni)
                .setContentSettingEnabled(
                        eq(mMockProfile),
                        eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT),
                        anyBoolean());
        Mockito.doAnswer(invocation -> mIsGlobalSettingsEnabled)
                .when(mMockWebsitePreferenceBridgeJni)
                .isContentSettingEnabled(
                        eq(mMockProfile), eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT));
        Mockito.doAnswer(
                        invocation -> {
                            mIsAutoDarkEnabledForUrlContentSettingValue =
                                    (int) invocation.getArguments()[4];
                            return null;
                        })
                .when(mMockWebsitePreferenceBridgeJni)
                .setContentSettingDefaultScope(
                        eq(mMockProfile),
                        eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT),
                        notNull(),
                        notNull(),
                        anyInt());
        Mockito.doAnswer(invocation -> mIsAutoDarkEnabledForUrlContentSettingValue)
                .when(mMockWebsitePreferenceBridgeJni)
                .getContentSetting(
                        eq(mMockProfile),
                        eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT),
                        notNull(),
                        notNull());
    }

    @After
    public void tearDown() {
        ShadowColorUtils.sInNightMode = false;
    }

    @Test
    public void testFeatureEnabled() {
        ShadowColorUtils.sInNightMode = true;
        mIsGlobalSettingsEnabled = true;
        Assert.assertTrue(
                "Feature should be enabled, if both global settings and night mode enabled.",
                WebContentsDarkModeController.isFeatureEnabled(mMockContext, mMockProfile));
        assertEnabledState(GURL.emptyGURL(), true);
    }

    @Test
    public void testFeatureEnabled_LightMode() {
        ShadowColorUtils.sInNightMode = false;
        mIsGlobalSettingsEnabled = true;
        Assert.assertFalse(
                "Feature should be disabled when not in night mode.",
                WebContentsDarkModeController.isFeatureEnabled(mMockContext, mMockProfile));
        assertEnabledState(GURL.emptyGURL(), false);
    }

    @Test
    public void testFeatureEnabled_NoUserSettings() {
        ShadowColorUtils.sInNightMode = true;
        mIsGlobalSettingsEnabled = false;
        Assert.assertFalse(
                "Feature should be disabled when global settings disabled.",
                WebContentsDarkModeController.isFeatureEnabled(mMockContext, mMockProfile));
        assertEnabledState(GURL.emptyGURL(), false);
    }

    private void doTestSetAutoDarkGlobalSettingsEnabled(boolean enabled) {
        WebContentsDarkModeController.setGlobalUserSettings(mMockProfile, enabled);
        Assert.assertEquals(
                "Auto dark settings state incorrect.",
                enabled,
                WebContentsDarkModeController.isGlobalUserSettingsEnabled(mMockProfile));
        assertAutoDarkModeChangeSourceRecorded(
                AutoDarkSettingsChangeSource.THEME_SETTINGS, enabled, 1);
        assertAutoDarkModeChangeSourceRecorded(
                AutoDarkSettingsChangeSource.SITE_SETTINGS_GLOBAL, enabled, 0);
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
        Mockito.doReturn(ContentSettingValues.ALLOW)
                .when(mMockWebsitePreferenceBridgeJni)
                .getDefaultContentSetting(
                        eq(mMockProfile), eq(ContentSettingsType.AUTO_DARK_WEB_CONTENT));

        WebContentsDarkModeController.setEnabledForUrl(mMockProfile, mMockGurl, enableForUrl);

        Assert.assertEquals(
                "Auto dark for URL is incorrect.",
                enableForUrl,
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
    public void testGetEnableStateForUrl_Enabled() {
        ShadowColorUtils.sInNightMode = true;
        mIsGlobalSettingsEnabled = true;
        mIsAutoDarkEnabledForUrlContentSettingValue = ContentSettingValues.ALLOW;
        assertEnabledState(mMockGurl, true);
    }

    @Test
    public void testGetEnableStateForUrl_Disabled() {
        ShadowColorUtils.sInNightMode = true;
        mIsGlobalSettingsEnabled = true;
        mIsAutoDarkEnabledForUrlContentSettingValue = ContentSettingValues.BLOCK;
        assertEnabledState(mMockGurl, false);
    }

    private void assertAutoDarkModeChangeSourceRecorded(
            @AutoDarkSettingsChangeSource int source, boolean enabled, int expectedCounts) {
        String histogramName =
                "Android.DarkTheme.AutoDarkMode.SettingsChangeSource."
                        + (enabled ? "Enabled" : "Disabled");
        int actualCount = RecordHistogram.getHistogramValueCountForTesting(histogramName, source);
        Assert.assertEquals(
                "Histogram <"
                        + histogramName
                        + "> for sample <"
                        + source
                        + "> is not recorded correctly.",
                expectedCounts,
                actualCount);
    }

    private void assertEnabledState(GURL url, boolean expectedEnabledState) {
        boolean actualEnabledState =
                WebContentsDarkModeController.getEnabledState(mMockProfile, mMockContext, url);
        Assert.assertEquals(
                "AutoDarkModeEnabledState does not match.",
                expectedEnabledState,
                actualEnabledState);
    }
}
