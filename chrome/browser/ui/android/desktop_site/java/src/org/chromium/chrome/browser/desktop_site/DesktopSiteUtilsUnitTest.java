// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_site;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Window;
import android.view.WindowManager;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ContextUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.desktop_site.DesktopSiteUtilsUnitTest.ShadowDisplayAndroid;
import org.chromium.chrome.browser.desktop_site.DesktopSiteUtilsUnitTest.ShadowDisplayAndroidManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettingsConstants;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link DesktopSiteUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowDisplayAndroid.class, ShadowDisplayAndroidManager.class})
public class DesktopSiteUtilsUnitTest {
    @Implements(DisplayAndroid.class)
    static class ShadowDisplayAndroid {
        private static DisplayAndroid sDisplayAndroid;

        public static void setDisplayAndroid(DisplayAndroid displayAndroid) {
            sDisplayAndroid = displayAndroid;
        }

        @Implementation
        public static DisplayAndroid getNonMultiDisplay(Context context) {
            return sDisplayAndroid;
        }
    }

    @Implements(DisplayAndroidManager.class)
    static class ShadowDisplayAndroidManager {
        private static Display sDisplay;

        public static void setDisplay(Display display) {
            sDisplay = display;
        }

        @Implementation
        public static Display getDefaultDisplayForContext(Context context) {
            return sDisplay;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextWrapperTestRule =
            new OverrideContextWrapperTestRule();

    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock private Activity mActivity;
    @Mock private Window mWindow;
    @Mock private WindowManager.LayoutParams mLayoutParams;
    @Mock private DisplayMetrics mDisplayMetrics;
    @Mock private Profile mProfile;
    @Mock private DisplayAndroid mDisplayAndroid;
    @Mock private Display mDisplay;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;

    private @ContentSetting int mRdsDefaultValue;
    private boolean mWindowSetting;
    private SharedPreferencesManager mSharedPreferencesManager;

    private final Map<String, Integer> mContentSettingMap = new HashMap<>();
    private final GURL mGoogleUrl = JUnitTestGURLs.GOOGLE_URL;
    private final GURL mMapsUrl = JUnitTestGURLs.MAPS_URL;

    private Resources mResources;

    private static final String ANY_SUBDOMAIN_PATTERN = "[*.]";
    private static final String GOOGLE_COM = "[*.]google.com/";
    private ShadowPackageManager mShadowPackageManager;
    private boolean mIsDefaultValuePreference;

    @Before
    public void setup() {
        WebsitePreferenceBridgeJni.setInstanceForTesting(mWebsitePreferenceBridgeJniMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        doAnswer(invocation -> mRdsDefaultValue)
                .when(mWebsitePreferenceBridgeJniMock)
                .getDefaultContentSetting(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE));

        doAnswer(
                        invocation -> {
                            mRdsDefaultValue =
                                    invocation.getArgument(2)
                                            ? ContentSetting.ALLOW
                                            : ContentSetting.BLOCK;
                            return null;
                        })
                .when(mWebsitePreferenceBridgeJniMock)
                .setContentSettingEnabled(
                        any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE), anyBoolean());

        doAnswer(
                        invocation -> {
                            mContentSettingMap.put(
                                    invocation.getArgument(2), invocation.getArgument(4));
                            return null;
                        })
                .when(mWebsitePreferenceBridgeJniMock)
                .setContentSettingCustomScope(
                        any(),
                        eq(ContentSettingsType.REQUEST_DESKTOP_SITE),
                        anyString(),
                        anyString(),
                        anyInt());
        doAnswer(invocation -> toDomainWildcardPattern(invocation.getArgument(0)))
                .when(mWebsitePreferenceBridgeJniMock)
                .toDomainWildcardPattern(anyString());

        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();

        mResources = ApplicationProvider.getApplicationContext().getResources();
        mResources.getConfiguration().smallestScreenWidthDp = 600;
        when(mActivity.getResources()).thenReturn(mResources);

        SysUtils.setAmountOfPhysicalMemoryKbForTesting(
                7000 * ConversionUtils.KILOBYTES_PER_MEGABYTE);
        ShadowDisplayAndroid.setDisplayAndroid(mDisplayAndroid);
        when(mDisplayAndroid.getDisplayWidth()).thenReturn(1600);
        when(mDisplayAndroid.getDisplayHeight()).thenReturn(2560);
        when(mDisplayAndroid.getXdpi()).thenReturn(275.5f);
        when(mDisplayAndroid.getYdpi()).thenReturn(276.5f);
        ShadowDisplayAndroidManager.setDisplay(mDisplay);
        when(mDisplay.getDisplayId()).thenReturn(Display.DEFAULT_DISPLAY);
        DisplayUtil.setCurrentSmallestScreenWidthForTesting(800);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        doAnswer(invocation -> mWindowSetting)
                .when(mPrefService)
                .getBoolean(eq(PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED));
        doAnswer(invocation -> mIsDefaultValuePreference)
                .when(mPrefService)
                .isDefaultValuePreference(eq(PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED));
        doAnswer(
                        invocation -> {
                            mWindowSetting = invocation.getArgument(1);
                            return true;
                        })
                .when(mPrefService)
                .setBoolean(eq(PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED), anyBoolean());
        doReturn(true)
                .when(mWebsitePreferenceBridgeJniMock)
                .isContentSettingGlobal(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE, mGoogleUrl, mGoogleUrl);
        doReturn(ContentSetting.DEFAULT)
                .when(mWebsitePreferenceBridgeJniMock)
                .getContentSetting(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE, mGoogleUrl, mGoogleUrl);
        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mWindow.getAttributes()).thenReturn(mLayoutParams);
        mLayoutParams.width = -1;
        mDisplayMetrics.density = 1.0f;
        mDisplayMetrics.widthPixels = 800;
        mShadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ false);
        DesktopSiteUtils.setTestDisplayMetrics(mDisplayMetrics);
    }

    @After
    public void tearDown() {
        ShadowDisplayAndroid.setDisplayAndroid(null);
        if (mSharedPreferencesManager != null) {
            mSharedPreferencesManager.removeKey(
                    ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING);
            mSharedPreferencesManager.removeKey(
                    SingleCategorySettingsConstants
                            .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY);
        }
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_Incognito() {
        // Incognito profile type.
        when(mProfile.isOffTheRecord()).thenReturn(true);
        when(mProfile.isPrimaryOtrProfile()).thenReturn(true);
        mRdsDefaultValue = ContentSetting.BLOCK;

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_Incognito() {
        // Incognito profile type.
        when(mProfile.isOffTheRecord()).thenReturn(true);
        when(mProfile.isPrimaryOtrProfile()).thenReturn(true);
        mRdsDefaultValue = ContentSetting.ALLOW;

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteBlock_WindowSettingOn() {
        mWindowSetting = true;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSetting.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSetting.BLOCK);
        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should not be removed "
                        + "when window setting is ON.",
                ContentSetting.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteBlock_WindowSettingOff() {
        mWindowSetting = false;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSetting.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSetting.BLOCK);
        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should be removed "
                        + "when window setting is OFF.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteAllow_WindowSettingOn() {
        mWindowSetting = true;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSetting.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSetting.ALLOW);
        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should not be removed "
                        + "when window setting is ON.",
                ContentSetting.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteAllow_WindowSettingOff() {
        mWindowSetting = false;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSetting.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSetting.ALLOW);
        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should be removed "
                        + "when window setting is OFF.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteAllow_WindowSettingOn() {
        mWindowSetting = true;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSetting.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSetting.ALLOW);
        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should not be removed "
                        + "when window setting is ON.",
                ContentSetting.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteAllow_WindowSettingOff() {
        mWindowSetting = false;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSetting.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSetting.ALLOW);
        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should be removed "
                        + "when window setting is OFF.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteBlock_WindowSettingOn() {
        mWindowSetting = true;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSetting.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSetting.BLOCK);
        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should not be removed "
                        + "when window setting is ON.",
                ContentSetting.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteBlock_WindowSettingOff() {
        mWindowSetting = false;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSetting.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSetting.BLOCK);
        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should be removed "
                        + "when window setting is OFF.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSetting.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        DesktopSiteUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSetting.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    /**
     * Helper to get domain wildcard pattern from URL. The real implementation calls {@link
     * WebsitePreferenceBridge}.
     *
     * @param origin A URL.
     * @return The domain wildcard pattern from the given URL.
     */
    private String toDomainWildcardPattern(String origin) {
        return ANY_SUBDOMAIN_PATTERN + origin.replaceAll(".*\\.(.+\\.[^.]+$)", "$1");
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_IsAndroidDesktop() {
        mOverrideContextWrapperTestRule.setIsDesktop(true);
        SysUtils.setAmountOfPhysicalMemoryKbForTesting(
                4000 * ConversionUtils.KILOBYTES_PER_MEGABYTE);
        boolean shouldDefaultEnable =
                DesktopSiteUtils.shouldDefaultEnableGlobalSetting(11, mActivity);
        Assert.assertTrue(
                "Desktop site global setting should be default-enabled on desktop "
                        + "Android, even for low memory",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_MemoryThreshold() {
        SysUtils.setAmountOfPhysicalMemoryKbForTesting(
                6000 * ConversionUtils.KILOBYTES_PER_MEGABYTE);
        boolean shouldDefaultEnable =
                DesktopSiteUtils.shouldDefaultEnableGlobalSetting(
                        DesktopSiteUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled on devices below the "
                        + "memory threshold.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_CustomScreenSizeThreshold() {
        boolean shouldDefaultEnable =
                DesktopSiteUtils.shouldDefaultEnableGlobalSetting(11.0, mActivity);
        Assert.assertTrue(
                "Desktop site global setting should be default-enabled on 10\"+ " + "devices.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_WithSmallDisplay() {
        boolean shouldDefaultEnable =
                DesktopSiteUtils.shouldDefaultEnableGlobalSetting(7, mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled", shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_ExternalDisplay() {
        when(mDisplay.getDisplayId()).thenReturn(/*non built-in display*/ 2);
        boolean shouldDefaultEnable =
                DesktopSiteUtils.shouldDefaultEnableGlobalSetting(
                        DesktopSiteUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled on external display",
                shouldDefaultEnable);

        when(mDisplay.getDisplayId()).thenReturn(Display.DEFAULT_DISPLAY);
        shouldDefaultEnable =
                DesktopSiteUtils.shouldDefaultEnableGlobalSetting(
                        DesktopSiteUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mActivity);
        Assert.assertTrue(
                "Desktop site global setting should be default-enabled on built-in display",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_UserPreviouslyUpdatedSetting() {
        // This SharedPreference key will ideally be updated when the user explicitly requests for
        // an update to the desktop site global setting.
        mSharedPreferencesManager.writeBoolean(
                SingleCategorySettingsConstants
                        .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY,
                true);
        boolean shouldDefaultEnable =
                DesktopSiteUtils.shouldDefaultEnableGlobalSetting(
                        DesktopSiteUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled if it has been "
                        + "previously updated by the user.",
                shouldDefaultEnable);
    }

    @Test
    public void testMaybeDefaultEnableGlobalSetting() {
        boolean didDefaultEnable =
                DesktopSiteUtils.maybeDefaultEnableGlobalSetting(
                        DesktopSiteUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mProfile,
                        mActivity);
        Assert.assertTrue(
                "Desktop site global setting should be default-enabled on big screen devices.",
                didDefaultEnable);
        Assert.assertEquals(
                "Desktop site content setting should be set correctly.",
                ContentSetting.ALLOW,
                mRdsDefaultValue);
        Assert.assertTrue(
                "SharedPreference DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING should be true.",
                mSharedPreferencesManager.contains(
                                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING)
                        && mSharedPreferencesManager.readBoolean(
                                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING,
                                false));

        // Verify that the desktop site global setting will be default-enabled at most once.
        boolean shouldDefaultEnable =
                DesktopSiteUtils.shouldDefaultEnableGlobalSetting(
                        DesktopSiteUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled more than once.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldApplyWindowSetting_IsAutomotive() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ true);
        mWindowSetting = true;
        boolean shouldApplyWindowSetting =
                DesktopSiteUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse(
                "Desktop site window setting should not be applied on automotive",
                shouldApplyWindowSetting);
    }

    @Test
    public void testShouldApplyWindowSetting_SettingOff() {
        mWindowSetting = false;
        boolean shouldApplyWindowSetting =
                DesktopSiteUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse(
                "Desktop site window setting should not be applied when window setting is off",
                shouldApplyWindowSetting);
    }

    @Test
    public void testShouldApplyWindowSetting_isNotGlobalSetting() {
        mWindowSetting = true;
        doReturn(false)
                .when(mWebsitePreferenceBridgeJniMock)
                .isContentSettingGlobal(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE, mGoogleUrl, mGoogleUrl);
        boolean shouldApplyWindowSetting =
                DesktopSiteUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse(
                "Desktop site window setting should not be applied when the current RDS setting "
                        + "is domain setting",
                shouldApplyWindowSetting);
    }

    @Test
    public void testShouldApplyWindowSetting_windowAttributesWidthValid() {
        mWindowSetting = true;
        mLayoutParams.width = 800;
        boolean shouldApplyWindowSetting =
                DesktopSiteUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse(
                "Desktop site window setting should not be applied when window width in dp is "
                        + "larger than 600",
                shouldApplyWindowSetting);

        mLayoutParams.width = 400;
        shouldApplyWindowSetting =
                DesktopSiteUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertTrue(
                "Desktop site window setting should be applied when window width in dp is "
                        + "smaller than 600",
                shouldApplyWindowSetting);
    }

    @Test
    public void testShouldApplyWindowSetting_windowAttributesWidthInvalid() {
        mWindowSetting = true;
        mDisplayMetrics.density = 2.0f;
        mDisplayMetrics.widthPixels = 1600;
        boolean shouldApplyWindowSetting =
                DesktopSiteUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse(
                "Desktop site window setting should not be applied when window width in dp is "
                        + "larger than 600",
                shouldApplyWindowSetting);

        mDisplayMetrics.widthPixels = 1000;
        shouldApplyWindowSetting =
                DesktopSiteUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertTrue(
                "Desktop site window setting should be applied when window width in dp is "
                        + "smaller than 600",
                shouldApplyWindowSetting);
    }

    @Test
    public void testMaybeDefaultEnableWindowSetting_PhoneSizedScreen() {
        mWindowSetting = false;
        mIsDefaultValuePreference = true;
        DisplayUtil.setCurrentSmallestScreenWidthForTesting(400);
        DesktopSiteUtils.maybeDefaultEnableWindowSetting(mActivity, mProfile);
        Assert.assertFalse(
                "Desktop site window setting should not be default enabled when the smallest "
                        + "screen width is less than 600dp",
                mWindowSetting);
    }

    @Test
    public void testMaybeDefaultEnableWindowSetting_DesktopAndroid() {
        mWindowSetting = false;
        mIsDefaultValuePreference = true;
        mOverrideContextWrapperTestRule.setIsDesktop(true);
        DesktopSiteUtils.maybeDefaultEnableWindowSetting(mActivity, mProfile);
        Assert.assertFalse(
                "Desktop site window setting should not be default enabled for desktop "
                        + "Android",
                mWindowSetting);
    }

    @Test
    public void testMaybeDefaultEnableWindowSetting_ExternalDisplay() {
        mWindowSetting = false;
        mIsDefaultValuePreference = true;
        when(mDisplay.getDisplayId()).thenReturn(/*non built-in display*/ 2);
        DesktopSiteUtils.maybeDefaultEnableWindowSetting(mActivity, mProfile);
        Assert.assertFalse(
                "Desktop site window setting should not be default enabled when Chrome is opened "
                        + "on external display",
                mWindowSetting);
    }

    @Test
    public void testMaybeDefaultEnableWindowSetting_NotDefaultValuePreference() {
        mWindowSetting = false;
        mIsDefaultValuePreference = false;
        DesktopSiteUtils.maybeDefaultEnableWindowSetting(mActivity, mProfile);
        Assert.assertFalse(
                "Desktop site window setting should not be default enabled when the preference "
                        + "has been previously changed",
                mWindowSetting);
    }

    @Test
    public void testMaybeDefaultEnableWindowSetting_ShouldDefaultEnable() {
        mWindowSetting = false;
        mIsDefaultValuePreference = true;
        DesktopSiteUtils.maybeDefaultEnableWindowSetting(mActivity, mProfile);
        Assert.assertTrue("Desktop site window setting should be default enabled", mWindowSetting);
    }

    @Test
    public void testShouldOverrideDesktopSite_contentSettingOn() {
        doReturn(ContentSetting.ALLOW)
                .when(mWebsitePreferenceBridgeJniMock)
                .getContentSetting(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE, mGoogleUrl, mGoogleUrl);
        boolean shouldOverride =
                DesktopSiteUtils.shouldOverrideDesktopSite(mProfile, mGoogleUrl, mActivity);
        Assert.assertTrue("Desktop site should be overridden.", shouldOverride);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.DESKTOP_UA_ON_CONNECTED_DISPLAY
                    + ":ext_display_desktop_ua_oem_allowlist/samsung")
    public void testShouldOverrideDesktopSite_onEligibleExternalDisplay() {
        when(mDisplay.getDisplayId()).thenReturn(/*non built-in display*/ 2);
        String originalManufacturer = Build.MANUFACTURER;
        try {
            ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "samsung");
            boolean shouldOverride =
                    DesktopSiteUtils.shouldOverrideDesktopSite(mProfile, mGoogleUrl, mActivity);
            Assert.assertTrue("Desktop site should be overridden.", shouldOverride);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", originalManufacturer);
            DesktopSiteUtils.sDesktopUAAllowedOnExternalDisplayForOem = null;
        }
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.DESKTOP_UA_ON_CONNECTED_DISPLAY
                    + ":ext_display_desktop_ua_oem_allowlist/samsung")
    public void
            testShouldOverrideDesktopSite_onEligibleExternalDisplay_userPreviouslyUpdatedSetting() {
        when(mDisplay.getDisplayId()).thenReturn(/*non built-in display*/ 2);
        String originalManufacturer = Build.MANUFACTURER;
        try {
            ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "samsung");
            mSharedPreferencesManager.writeBoolean(
                    SingleCategorySettingsConstants
                            .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY,
                    true);
            boolean shouldOverride =
                    DesktopSiteUtils.shouldOverrideDesktopSite(mProfile, mGoogleUrl, mActivity);
            Assert.assertFalse("Desktop site should not be overridden.", shouldOverride);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", originalManufacturer);
            DesktopSiteUtils.sDesktopUAAllowedOnExternalDisplayForOem = null;
        }
    }

    @Test
    public void testShouldOverrideDesktopSite_defaultDisplay_shouldNotOverride() {
        when(mDisplay.getDisplayId()).thenReturn(Display.DEFAULT_DISPLAY);
        boolean shouldOverride =
                DesktopSiteUtils.shouldOverrideDesktopSite(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse("Desktop site should not be overridden.", shouldOverride);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.DESKTOP_UA_ON_CONNECTED_DISPLAY
                    + ":ext_display_desktop_ua_oem_allowlist/samsung")
    public void testShouldOverrideDesktopSite_OEMNotAllowlisted_shouldNotOverride() {
        when(mDisplay.getDisplayId()).thenReturn(/*non built-in display*/ 2);
        String originalManufacturer = Build.MANUFACTURER;
        try {
            ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "something_else");
            boolean shouldOverride =
                    DesktopSiteUtils.shouldOverrideDesktopSite(mProfile, mGoogleUrl, mActivity);
            Assert.assertFalse("Desktop site should not be overridden.", shouldOverride);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", originalManufacturer);
            DesktopSiteUtils.sDesktopUAAllowedOnExternalDisplayForOem = null;
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_UA_ON_CONNECTED_DISPLAY)
    public void testShouldOverrideDesktopSite_OEMAllowlistNotSet_shouldOverride() {
        when(mDisplay.getDisplayId()).thenReturn(/*non built-in display*/ 2);
        String originalManufacturer = Build.MANUFACTURER;
        try {
            ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "samsung");
            boolean shouldOverride =
                    DesktopSiteUtils.shouldOverrideDesktopSite(mProfile, mGoogleUrl, mActivity);
            Assert.assertTrue("Desktop site should be overridden.", shouldOverride);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", originalManufacturer);
            DesktopSiteUtils.sDesktopUAAllowedOnExternalDisplayForOem = null;
        }
    }
}
