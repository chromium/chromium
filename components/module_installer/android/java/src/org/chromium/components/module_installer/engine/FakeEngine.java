// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

import android.content.Context;
import android.content.pm.PackageManager;

import com.google.android.play.core.splitcompat.SplitCompat;
import com.google.android.play.core.splitcompat.ingestion.Verifier;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Engine that looks for module APKs on the device's disk instead of invoking the Play core API to
 * install a feature module. This backend is used for testing purposes where the module is not
 * uploaded to the Play server.
 */
class FakeEngine extends SplitCompatEngine {
    private static final String TAG = "FakeEngine";
    private static final String MODULES_SRC_DIRECTORY_PATH = "/data/local/tmp/modules";

    @Override
    public void installDeferred(String moduleName) {
        // This is currently not supported by fake installs.
    }

    /**
     * Copies {MODULES_SRC_DIRECTORY_PATH}/|moduleName|.apk to the folder where SplitCompat expects
     * downloaded modules to be. Then calls SplitCompat to emulate the module.
     *
     * We copy the module so that this works on non-rooted devices. The path SplitCompat expects
     * module to be is not accessible without rooting.
     */
    @Override
    public void install(String moduleName, InstallListener listener) {
        ThreadUtils.assertOnUiThread();

        new AsyncTask<Boolean>() {
            @Override
            protected Boolean doInBackground() {
                return installInternal(moduleName);
            }

            @Override
            protected void onPostExecute(Boolean success) {
                notifyListener(listener, success);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private boolean installInternal(String moduleName) {
        Context context = ContextUtils.getApplicationContext();
        long versionCode = BuildInfo.getInstance().versionCode;

        // Get list of all files at path where SplitCompat looks for downloaded modules.
        // May change in future releases of the Play Core SDK.
        File srcModuleDir = new File(MODULES_SRC_DIRECTORY_PATH);
        if (!srcModuleDir.exists()) {
            Log.e(TAG, "Modules source directory does not exist");
            return false;
        }
        if (!srcModuleDir.canRead()) {
            Log.e(TAG, "Cannot read modules source directory");
            return false;
        }
        if (!srcModuleDir.isDirectory()) {
            Log.e(TAG, "Modules source directory is not a directory");
            return false;
        }
        File[] srcModuleFiles = srcModuleDir.listFiles();
        if (srcModuleFiles == null) {
            Log.e(TAG, "Cannot get list of files in modules source directory");
            return false;
        }

        // Check if any apks for the module are actually installed.
        boolean no_module_apks_installed = true;

        for (File srcModuleFile : srcModuleFiles) {
            // Take only source APK files of the specified module.
            String srcModuleFileName = srcModuleFile.getName();
            if (srcModuleFileName.endsWith(".apk") && srcModuleFileName.startsWith(moduleName)) {
                // Construct destination file corresponding to each source file.
                File dstModuleFile = joinPaths(context.getFilesDir().getPath(), "splitcompat",
                        Long.toString(versionCode), "unverified-splits", srcModuleFileName);

                // NOTE: Need to give Chrome storage permission for this to work.
                try {
                    dstModuleFile.getParentFile().mkdirs();
                } catch (SecurityException e) {
                    Log.e(TAG, "Failed to create module dir %s", dstModuleFile.getName(), e);
                    return false;
                }

                try (FileInputStream istream = new FileInputStream(srcModuleFile);
                        FileOutputStream ostream = new FileOutputStream(dstModuleFile)) {
                    ostream.getChannel().transferFrom(
                            istream.getChannel(), 0, istream.getChannel().size());
                    if (srcModuleFileName.equals(moduleName + ".apk")) {
                        // Base apk of the module must be installed for install
                        // to be successful.
                        no_module_apks_installed = false;
                    }
                } catch (RuntimeException | IOException e) {
                    Log.e(TAG, "Failed to install module apk %s", dstModuleFile.getName(), e);
                    return false;
                }
            }
        }

        if (no_module_apks_installed) {
            Log.e(TAG, "Did not find any module APKs");
            return false;
        }

        // Check that the module's signature matches Chrome's.
        try {
            Verifier verifier = new Verifier(context);
            if (!verifier.verifySplits()) {
                return false;
            }
        } catch (IOException | PackageManager.NameNotFoundException e) {
            return false;
        }

        // Tell SplitCompat to do a full emulation of the module.
        return SplitCompat.fullInstall(context);
    }

    private File joinPaths(String... paths) {
        File result = new File("");
        for (String path : paths) {
            result = new File(result, path);
        }
        return result;
    }
}
