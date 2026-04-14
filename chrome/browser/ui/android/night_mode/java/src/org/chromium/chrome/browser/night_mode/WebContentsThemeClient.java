// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.ColorUtils;

/** A client to provide dark mode information for a given WebContents. */
@NullMarked
@JNINamespace("night_mode")
public class WebContentsThemeClient {
    @CalledByNative
    public static boolean isNightModeEnabled(
            @JniType("content::WebContents*") WebContents webContents) {
        if (webContents == null) return false;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return false;
        Context context = window.getContext().get();
        if (context == null) return false;
        return ColorUtils.inNightMode(context);
    }

    @CalledByNative
    public static boolean isForceDarkWebContentEnabled(
            @JniType("content::WebContents*") WebContents webContents) {
        if (webContents == null) return false;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)) {
            return true;
        }
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
            return false;
        }
        Profile profile = Profile.fromWebContents(webContents);
        if (profile == null) return false;

        return isNightModeEnabled(webContents)
                && WebContentsDarkModeController.isEnabledForUrl(
                        profile, webContents.getVisibleUrl());
    }
}
