// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Application;
import android.content.Context;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.build.BuildConfig;
import org.chromium.ui.base.ResourceBundle;

/**
 * Entry point for the Android cast shell application. Handles initialization of information that
 * needs to be shared across the main activity and the child services created.
 */
public class CastApplication extends Application {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cast_shell";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        ContextUtils.initApplicationContext(this);
        ResourceBundle.setAvailablePakLocales(ProductConfig.LOCALES);
        LibraryLoader.getInstance().setLinkerImplementation(ProductConfig.USE_CHROMIUM_LINKER);
        LibraryLoader.getInstance().setLibraryProcessType(isBrowserProcess()
                        ? LibraryProcessType.PROCESS_BROWSER
                        : LibraryProcessType.PROCESS_CHILD);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        ApplicationStatus.initialize(this);
    }

    private static boolean isBrowserProcess() {
        return BuildConfig.IS_BUNDLE
                ? ContextUtils.getProcessName().contains("cast_browser_process")
                : !ContextUtils.getProcessName().contains(":");
    }
}
