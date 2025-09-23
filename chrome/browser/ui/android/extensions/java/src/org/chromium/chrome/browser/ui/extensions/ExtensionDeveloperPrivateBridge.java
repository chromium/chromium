// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.content.Context;
import android.os.Bundle;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;

@NullMarked
@JNINamespace("extensions")
public class ExtensionDeveloperPrivateBridge {
    private ExtensionDeveloperPrivateBridge() {}

    @CalledByNative
    static void showSiteSettings(@JniType("std::string") String extensionId) {
        Context context = ContextUtils.getApplicationContext();

        String url = UrlConstants.CHROME_EXTENSION_SCHEME + "://" + extensionId;
        Bundle args = SingleWebsiteSettings.createFragmentArgsForExtensionSite(url);

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(context, SingleWebsiteSettings.class, args);
    }
}
