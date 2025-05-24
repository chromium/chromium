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
        ResolveInfo info = getDefaultWebBrowserActivityResolveInfo();
        if (info == null) {
            return false;
        }

        int state = getCurrentDefaultBrowserState(info);
        if (state == DefaultBrowserState.CHROME_DEFAULT) {
            return false;
        } else if (state == DefaultBrowserState.NO_DEFAULT) {
            // Criteria 2
            return !isChromeStable() || !isChromePreStableInstalled();
        } else { // other default
            // Criteria 1
            return !isCurrentDefaultBrowserChrome(info);
        }
    }

    public @DefaultBrowserState int getCurrentDefaultBrowserState() {
        ResolveInfo info = PackageManagerUtils.resolveDefaultWebBrowserActivity();
        return getCurrentDefaultBrowserState(info);
    }

    boolean isCurrentDefaultBrowserChrome(ResolveInfo info) {
        String packageName = info.activityInfo.packageName;
        for (String name : CHROME_PACKAGE_NAMES) {
            if (name.equals(packageName)) return true;
        }
        return false;
    }

    @DefaultBrowserState
    int getCurrentDefaultBrowserState(@Nullable ResolveInfo info) {
        if (info == null || info.match == 0) return DefaultBrowserState.NO_DEFAULT; // no default
        if (TextUtils.equals(
                ContextUtils.getApplicationContext().getPackageName(),
                info.activityInfo.packageName)) {
            return DefaultBrowserState.CHROME_DEFAULT; // Already default
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
