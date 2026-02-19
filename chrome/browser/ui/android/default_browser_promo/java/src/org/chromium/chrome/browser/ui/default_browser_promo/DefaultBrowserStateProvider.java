// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.DefaultBrowserInfo;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultBrowserState;

/**
 * A utility class providing information regarding the default browser states of the system to
 * facilitate testing and interacting with external states by {@link PackageManagerUtils}
 */
@NullMarked
public class DefaultBrowserStateProvider {
    static final String CHROME_STABLE_PACKAGE_NAME = "com.android.chrome";

    // TODO(crbug.com/40697015): move to some util class for reuse.
    static final String[] CHROME_PACKAGE_NAMES = {
        CHROME_STABLE_PACKAGE_NAME,
        "org.chromium.chrome",
        "com.chrome.canary",
        "com.chrome.beta",
        "com.chrome.dev"
    };

    /**
     * This decides whether the promo should be promoted base on the current default browser state.
     * Return false if any of following criteria is met:
     *      1. Any chrome, including pre-stable, has been set as default.
     *      2. On Chrome stable, no default browser is set and multiple chrome channels are
     *         installed.
     *
     * @return boolean if promo dialog can be displayed.
     */
    public boolean shouldShowPromo() {
        int state = getCurrentDefaultBrowserState(true);
        if (state == DefaultBrowserState.CHROME_DEFAULT) {
            return false;
        } else if (state == DefaultBrowserState.NO_DEFAULT) {
            // Criteria 2
            return !isChromeStable() || !isChromePreStableInstalled();
        } else { // other default
            // Criteria 1
            return state != DefaultBrowserState.OTHER_CHROME_DEFAULT;
        }
    }

    public @DefaultBrowserState int getCurrentDefaultBrowserState() {
        return getCurrentDefaultBrowserState(false);
    }

    /**
     * Gets the current default browser state. After {@link
     * ChromeFeatureList.sDefaultBrowserPromoEntryPoint} enable by default, The parameter will be
     * deleted and the no the no-arg method can also be removed.
     *
     * @param needIdentifyOtherChromeDefault Determines whether the result includes
     *     OTHER_CHROME_DEFAULT.
     * @return The current default browser state.
     */
    public @DefaultBrowserState int getCurrentDefaultBrowserState(
            boolean needIdentifyOtherChromeDefault) {
        DefaultBrowserInfo.DefaultInfo defaultBrowserInfo =
                DefaultBrowserInfo.getDefaultBrowserInfoCacheResult();
        return defaultBrowserInfo != null
                ? defaultBrowserInfo.defaultBrowserState
                : getCurrentDefaultBrowserState(
                        getDefaultWebBrowserActivityResolveInfo(), needIdentifyOtherChromeDefault);
    }

    @DefaultBrowserState
    int getCurrentDefaultBrowserState(
            @Nullable ResolveInfo info, boolean needIdentifyOtherChromeDefault) {
        if (info == null || info.match == 0) return DefaultBrowserState.NO_DEFAULT; // no default

        String defaultPackage = info.activityInfo.packageName;
        String myPackage = ContextUtils.getApplicationContext().getPackageName();
        if (TextUtils.equals(myPackage, defaultPackage)) {
            return DefaultBrowserState.CHROME_DEFAULT; // Already default
        }

        //  Check if it is a different Chrome (e.g. the user is in Canary, but Stable is the
        // default).
        if (ChromeFeatureList.sDefaultBrowserPromoEntryPoint.isEnabled()
                || needIdentifyOtherChromeDefault) {
            for (String chromePackage : CHROME_PACKAGE_NAMES) {
                if (chromePackage.equals(defaultPackage)) {
                    return DefaultBrowserState.OTHER_CHROME_DEFAULT;
                }
            }
        }

        return DefaultBrowserState.OTHER_DEFAULT;
    }

    boolean isChromeStable() {
        return ContextUtils.getApplicationContext()
                .getPackageName()
                .equals(CHROME_STABLE_PACKAGE_NAME);
    }

    boolean isChromePreStableInstalled() {
        for (ResolveInfo info : PackageManagerUtils.queryAllWebBrowsersInfo()) {
            for (String name : CHROME_PACKAGE_NAMES) {
                if (name.equals(CHROME_STABLE_PACKAGE_NAME)) continue;
                if (name.equals(info.activityInfo.packageName)) return true;
            }
        }
        return false;
    }

    @Nullable ResolveInfo getDefaultWebBrowserActivityResolveInfo() {
        return PackageManagerUtils.resolveDefaultWebBrowserActivity();
    }
}
