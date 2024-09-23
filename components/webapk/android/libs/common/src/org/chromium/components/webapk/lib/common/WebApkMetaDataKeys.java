// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapk.lib.common;

/** <meta-data> keys for WebAPK Android Manifest. */
public final class WebApkMetaDataKeys {
    public static final String SHELL_APK_VERSION = "org.chromium.webapk.shell_apk.shellApkVersion";
    public static final String RUNTIME_HOST = "org.chromium.webapk.shell_apk.runtimeHost";
    public static final String RUNTIME_HOST_APPLICATION_NAME =
            "org.chromium.webapk.shell_apk.runtimeHostApplicationName";
    public static final String START_URL = "org.chromium.webapk.shell_apk.startUrl";
    public static final String LOGGED_INTENT_URL_PARAM =
            "org.chromium.webapk.shell_apk.loggedIntentUrlParam";
    public static final String NAME = "org.chromium.webapk.shell_apk.name";
    public static final String SHORT_NAME = "org.chromium.webapk.shell_apk.shortName";
    public static final String HAS_CUSTOM_NAME = "org.chromium.webapk.shell_apk.hasCustomName";
    public static final String SCOPE = "org.chromium.webapk.shell_apk.scope";
    public static final String DISPLAY_MODE = "org.chromium.webapk.shell_apk.displayMode";
    public static final String ORIENTATION = "org.chromium.webapk.shell_apk.orientation";
    public static final String THEME_COLOR = "org.chromium.webapk.shell_apk.themeColor";
    public static final String BACKGROUND_COLOR = "org.chromium.webapk.shell_apk.backgroundColor";
    public static final String DARK_THEME_COLOR = "org.chromium.webapk.shell_apk.darkThemeColor";
    public static final String DARK_BACKGROUND_COLOR =
            "org.chromium.webapk.shell_apk.darkBackgroundColor";
    public static final String DEFAULT_BACKGROUND_COLOR_ID =
            "org.chromium.webapk.shell_apk.defaultBackgroundColorId";
    public static final String ICON_ID = "org.chromium.webapk.shell_apk.iconId";
    public static final String MASKABLE_ICON_ID = "org.chromium.webapk.shell_apk.maskableIconId";
    public static final String SPLASH_ID = "org.chromium.webapk.shell_apk.splashId";
    public static final String IS_SPLASH_ICON_MASKABLE_BOOLEAN_ID =
            "org.chromium.webapk.shell_apk.isSplashIconMaskableBooleanId";
    public static final String IS_NEW_STYLE_WEBAPK =
            "org.chromium.webapk.shell_apk.isNewStyleWebApk";

    public static final String ICON_URLS_AND_ICON_MURMUR2_HASHES =
            "org.chromium.webapk.shell_apk.iconUrlsAndIconMurmur2Hashes";
    public static final String WEB_MANIFEST_URL = "org.chromium.webapk.shell_apk.webManifestUrl";
    public static final String WEB_MANIFEST_ID = "org.chromium.webapk.shell_apk.webManifestId";
    public static final String APP_KEY = "org.chromium.webapk.shell_apk.appKey";
    public static final String DISTRIBUTOR = "org.chromium.webapk.shell_apk.distributor";
    public static final String SHARE_ACTION = "org.chromium.webapk.shell_apk.shareAction";
    public static final String SHARE_METHOD = "org.chromium.webapk.shell_apk.shareMethod";
    public static final String SHARE_ENCTYPE = "org.chromium.webapk.shell_apk.shareEnctype";
    public static final String SHARE_PARAM_TITLE = "org.chromium.webapk.shell_apk.shareParamTitle";
    public static final String SHARE_PARAM_TEXT = "org.chromium.webapk.shell_apk.shareParamText";
    public static final String SHARE_PARAM_NAMES = "org.chromium.webapk.shell_apk.shareParamNames";
    public static final String SHARE_PARAM_ACCEPTS =
            "org.chromium.webapk.shell_apk.shareParamAccepts";
    public static final String ENABLE_SITE_SETTINGS_SHORTCUT =
            "org.chromium.webapk.shell_apk.enableSiteSettingsShortcut";
}
