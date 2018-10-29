// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components_browsertests_apk;

import android.app.Application;
import android.content.Context;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildConfig;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.multidex.ChromiumMultiDexInstaller;

/**
 * A basic content_public.browser.tests {@link android.app.Application}.
 */
public class ComponentsBrowserTestsApplication extends Application {
    static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "components_shell";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        ContextUtils.initApplicationContext(this);
        // The test harness runs in the main process, and browser in :test_process.
        boolean isMainProcess = !ContextUtils.getProcessName().contains(":");
        boolean isBrowserProcess = ContextUtils.getProcessName().contains(":test");
        if (BuildConfig.IS_MULTIDEX_ENABLED && (isMainProcess || isBrowserProcess)) {
            ChromiumMultiDexInstaller.install(this);
        }
        if (isBrowserProcess) {
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
            ApplicationStatus.initialize(this);
        }
    }
}
