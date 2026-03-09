// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import org.chromium.build.annotations.NullMarked;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Holds package name constants for different Chrome variants on Android. This includes stable,
 * beta, dev, and canary releases of Chrome.
 */
@NullMarked
public final class ChromePackageNameVariant {
    public static final String CHROME_STABLE_PACKAGE_NAME = "com.android.chrome";
    public static final Set<String> CHROME_PRE_STABLE_PACKAGE_NAMES =
            Set.of("org.chromium.chrome", "com.chrome.canary", "com.chrome.beta", "com.chrome.dev");
    public static final Set<String> CHROME_PACKAGE_NAMES;

    static {
        Set<String> all = new HashSet<>(CHROME_PRE_STABLE_PACKAGE_NAMES);
        all.add(CHROME_STABLE_PACKAGE_NAME);
        CHROME_PACKAGE_NAMES = Collections.unmodifiableSet(all);
    }

    private ChromePackageNameVariant() {}
}
