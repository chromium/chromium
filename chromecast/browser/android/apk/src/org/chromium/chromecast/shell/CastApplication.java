// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Application;
import android.content.Context;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.ui.base.ResourceBundle;

/**
 * Entry point for the Android cast shell application.  Handles initialization of information that
 * needs to be shared across the main activity and the child services created.
 *
 * Note that this gets run for each process, including sandboxed child render processes. Child
 * processes don't need most of the full "setup" performed in CastBrowserHelper.java, but they do
 * require a few basic pieces (found here).
 */
public class CastApplication extends Application {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cast_shell";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        ContextUtils.initApplicationContext(this);
        ResourceBundle.setAvailablePakLocales(
                ProductConfig.COMPRESSED_LOCALES, ProductConfig.UNCOMPRESSED_LOCALES);
        LibraryLoader.getInstance().setConfiguration(
                ProductConfig.USE_CHROMIUM_LINKER, ProductConfig.USE_MODERN_LINKER);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        ApplicationStatus.initialize(this);
    }
}
