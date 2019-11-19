// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_browsertests_apk;

import android.content.Context;

import org.chromium.base.PathUtils;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.native_test.NativeBrowserTestApplication;
import org.chromium.ui.base.ResourceBundle;

/**
 * A basic chrome.browser.tests {@link android.app.Application}.
 */
public class ChromeBrowserTestsApplication extends NativeBrowserTestApplication {
    static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";

    @Override
    protected void attachBaseContext(Context base) {
        boolean isBrowserProcess = isBrowserProcess();

        if (isBrowserProcess) UmaUtils.recordMainEntryPointTime();

        super.attachBaseContext(base);

        if (isBrowserProcess) {
            // Test-only stuff, see also NativeUnitTest.java.
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
            // ResourceBundle asserts that locale paks have been given to it.
            // In test targets there is no list of paks generated.
            ResourceBundle.setNoAvailableLocalePaks();
        }
    }
}
