// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.webapps;

import android.content.Intent;
import android.graphics.Color;

import org.chromium.base.TimeUtils;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.webapps.WebApkIntentDataProviderFactory;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.util.ColorUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/** Builder class for WebAPK {@link BrowserServicesIntentDataProvider} objects. */
public class WebApkIntentDataProviderBuilder {
    private String mWebApkPackageName;
    private String mUrl;
    private String mScope;
    private @DisplayMode.EnumType int mDisplayMode = DisplayMode.STANDALONE;
    private String mManifestUrl;
    private int mWebApkVersionCode;
    private String mManifestId;
    private String mName;
    private String mShortName;
    private WebappIcon mPrimaryIcon;
    private Map<String, String> mIconUrlToMurmur2HashMap = new HashMap<String, String>();
    private long mToolbarColor;
    private int mShellApkVersion = 1;

    public WebApkIntentDataProviderBuilder(String webApkPackageName, String url) {
        mWebApkPackageName = webApkPackageName;
        mUrl = url;
    }

    public WebApkIntentDataProviderBuilder setScope(String scope) {
        mScope = scope;
        return this;
    }

    public WebApkIntentDataProviderBuilder setDisplayMode(@DisplayMode.EnumType int displayMode) {
        mDisplayMode = displayMode;
        return this;
    }

    public WebApkIntentDataProviderBuilder setManifestUrl(String manifestUrl) {
        mManifestUrl = manifestUrl;
        return this;
    }

    public WebApkIntentDataProviderBuilder setWebApkVersionCode(int versionCode) {
        mWebApkVersionCode = versionCode;
        return this;
    }

    public WebApkIntentDataProviderBuilder setWebApkManifestId(String manifestId) {
        mManifestId = manifestId;
        return this;
    }

    public WebApkIntentDataProviderBuilder setName(String name) {
        mName = name;
        return this;
    }

    public WebApkIntentDataProviderBuilder setShortName(String shortName) {
        mShortName = shortName;
        return this;
    }

    public WebApkIntentDataProviderBuilder setToolbarColor(long color) {
        mToolbarColor = color;
        return this;
    }

    public WebApkIntentDataProviderBuilder setPrimaryIcon(WebappIcon icon) {
        mPrimaryIcon = icon;
        return this;
    }

    public WebApkIntentDataProviderBuilder setIconUrlToMurmur2HashMap(Map<String, String> hashmap) {
        mIconUrlToMurmur2HashMap = hashmap;
        return this;
    }

    public WebApkIntentDataProviderBuilder setShellApkVersion(int shellApkVersion) {
        mShellApkVersion = shellApkVersion;
        return this;
    }

    /** Builds {@link BrowserServicesIntentDataProvider} object using options that have been set. */
    public BrowserServicesIntentDataProvider build() {
        return WebApkIntentDataProviderFactory.create(
                new Intent(),
                mUrl,
                mScope,
                mPrimaryIcon,
                null,
                mName,
                mShortName,
                /* hasCustomName */ false,
                mDisplayMode,
                ScreenOrientationLockType.DEFAULT,
                ShortcutSource.UNKNOWN,
                mToolbarColor,
                ColorUtils.INVALID_COLOR,
                ColorUtils.INVALID_COLOR,
                ColorUtils.INVALID_COLOR,
                Color.WHITE,
                /* isPrimaryIconMaskable= */ false,
                /* isSplashIconMaskable= */ false,
                mWebApkPackageName,
                mShellApkVersion,
                mManifestUrl,
                mUrl,
                mManifestId,
                /* appKey= */ null,
                WebApkDistributor.BROWSER,
                mIconUrlToMurmur2HashMap,
                null,
                /* forceNavigation= */ false,
                /* isSplashProvidedByWebApk= */ false,
                null,
                /* shortcutItems= */ new ArrayList<>(),
                mWebApkVersionCode,
                /* lastUpdateTime= */ TimeUtils.currentTimeMillis());
    }
}
