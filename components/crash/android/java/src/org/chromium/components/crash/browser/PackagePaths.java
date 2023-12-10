// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash.browser;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;

import org.chromium.base.PackageUtils;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** This class builds paths for the Chrome package. */
public abstract class PackagePaths {
    // Prevent instantiation.
    private PackagePaths() {}

    /**
     * @ Build paths for the chrome/webview package for the purpose of loading CrashpadMain via
     * /system/bin/app_process.
     */
    @CalledByNative
    public static String[] makePackagePaths(String arch) {
        PackageInfo pi =
                PackageUtils.getApplicationPackageInfo(
                        PackageManager.GET_SHARED_LIBRARY_FILES
                                | PackageManager.MATCH_UNINSTALLED_PACKAGES);

        List<String> zipPaths = new ArrayList<>(10);
        zipPaths.add(pi.applicationInfo.sourceDir);
        if (pi.applicationInfo.splitSourceDirs != null) {
            Collections.addAll(zipPaths, pi.applicationInfo.splitSourceDirs);
        }

        if (pi.applicationInfo.sharedLibraryFiles != null) {
            Collections.addAll(zipPaths, pi.applicationInfo.sharedLibraryFiles);
        }

        List<String> libPaths = new ArrayList<>(10);
        File parent = new File(pi.applicationInfo.nativeLibraryDir).getParentFile();
        if (parent != null) {
            libPaths.add(new File(parent, arch).getPath());

            // arch is the currently loaded library's ABI name. This is the name of the library
            // directory in an APK, but may differ from the library directory extracted to the
            // filesystem. ARM family abi names have a suffix specifying the architecture
            // version, but may be extracted to directories named "arm64" or "arm".
            // crbug.com/930342
            if (arch.startsWith("arm64")) {
                libPaths.add(new File(parent, "arm64").getPath());
            } else if (arch.startsWith("arm")) {
                libPaths.add(new File(parent, "arm").getPath());
            }
        }
        for (String zip : zipPaths) {
            if (zip.endsWith(".apk")) {
                libPaths.add(zip + "!/lib/" + arch);
            }
        }
        libPaths.add(System.getProperty("java.library.path"));
        libPaths.add(pi.applicationInfo.nativeLibraryDir);

        return new String[] {
            TextUtils.join(File.pathSeparator, zipPaths),
            TextUtils.join(File.pathSeparator, libPaths)
        };
    }
}
