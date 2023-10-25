// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.webapps;

import android.content.Intent;
import android.graphics.Color;

import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.webapps.WebApkIntentDataProviderFactory;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.util.ColorUtils;

import java.util.ArrayList;
import java.util.HashMap;

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
    private long mToolbarColor;

    public WebApkIntentDataProviderBuilder(String webApkPackageName, String url) {
        mWebApkPackageName = webApkPackageName;
        mUrl = url;
    }

    public void setScope(String scope) {
        mScope = scope;
    }

    public void setDisplayMode(@DisplayMode.EnumType int displayMode) {
        mDisplayMode = displayMode;
    }

    public void setManifestUrl(String manifestUrl) {
        mManifestUrl = manifestUrl;
    }

    public void setWebApkVersionCode(int versionCode) {
        mWebApkVersionCode = versionCode;
    }

    public void setWebApkManifestId(String manifestId) {
        mManifestId = manifestId;
    }

    public void setName(String name) {
        mName = name;
    }

    public void setShortName(String shortName) {
        mShortName = shortName;
    }

    public void setToolbarColor(long color) {
        mToolbarColor = color;
    }

    private String manifestId() {
        if (mManifestId == null) {
            return mUrl;
        }
        return mManifestId;
    }

    /**
     * Builds {@link BrowserServicesIntentDataProvider} object using options that have been set.
     */
    public BrowserServicesIntentDataProvider build() {
        return WebApkIntentDataProviderFactory.create(
                new Intent(),
                mUrl,
                mScope,
                null,
                null,
                mName,
                mShortName,
                mDisplayMode,
                ScreenOrientationLockType.DEFAULT,
                ShortcutSource.UNKNOWN,
                mToolbarColor,
                ColorUtils.INVALID_COLOR,
                ColorUtils.INVALID_COLOR,
                ColorUtils.INVALID_COLOR,
                Color.WHITE,
                false /* isPrimaryIconMaskable */,
                false /* isSplashIconMaskable */,
                mWebApkPackageName, /* shellApkVersion */
                1,
                mManifestUrl,
                mUrl,
                manifestId(),
                null /*appKey*/,
                WebApkDistributor.BROWSER,
                new HashMap<String, String>() /* iconUrlToMurmur2HashMap */,
                null,
                false /* forceNavigation */,
                false /* isSplashProvidedByWebApk */,
                null,
                new ArrayList<>() /* shortcutItems */,
                mWebApkVersionCode);
    }
}
