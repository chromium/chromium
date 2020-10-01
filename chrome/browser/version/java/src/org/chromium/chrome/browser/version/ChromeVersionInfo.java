// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.version;

import org.chromium.components.version_info.Channel;

/**
 * A utility class for querying information about the current Chrome build.
 * Intentionally doesn't depend on native so that the data can be accessed before
 * libchrome.so is loaded.
 */
public class ChromeVersionInfo {
    /**
     * @return Whether this build is a local build.
     */
    public static boolean isLocalBuild() {
        return ChromeVersionConstants.CHANNEL == Channel.DEFAULT;
    }

    /**
     * @return Whether this build is a canary build.
     */
    public static boolean isCanaryBuild() {
        return ChromeVersionConstants.CHANNEL == Channel.CANARY;
    }

    /**
     * @return Whether this build is a dev build.
     */
    public static boolean isDevBuild() {
        return ChromeVersionConstants.CHANNEL == Channel.DEV;
    }

    /**
     * @return Whether this build is a beta build.
     */
    public static boolean isBetaBuild() {
        return ChromeVersionConstants.CHANNEL == Channel.BETA;
    }

    /**
     * @return Whether this build is a stable build.
     */
    public static boolean isStableBuild() {
        return ChromeVersionConstants.CHANNEL == Channel.STABLE;
    }

    /**
     * @return Whether this is an official (i.e. Google Chrome) build.
     */
    public static boolean isOfficialBuild() {
        return ChromeVersionConstants.IS_OFFICIAL_BUILD;
    }

    /**
     * @return The version number.
     */
    public static String getProductVersion() {
        return ChromeVersionConstants.PRODUCT_VERSION;
    }

    /**
     * @return The major version number.
     */
    public static int getProductMajorVersion() {
        return ChromeVersionConstants.PRODUCT_MAJOR_VERSION;
    }

    /**
     * @return The build number.
     */
    public static int getBuildVersion() {
        return ChromeVersionConstants.PRODUCT_BUILD_VERSION;
    }
}
