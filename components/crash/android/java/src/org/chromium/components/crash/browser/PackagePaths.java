// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash.browser;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;

import org.chromium.base.PackageUtils;
import org.chromium.build.annotations.NullMarked;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** This class builds paths for the Chrome package. */
@NullMarked
public abstract class PackagePaths {
    // Prevent instantiation.
    private PackagePaths() {}

    /**
     * Build paths for the chrome/webview package for the purpose of loading CrashpadMain.
     *
     * @param arch The target ABI directory name (e.g. "arm64-v8a" or "armeabi-v7a").
     * @param isSameBitness Whether {@code arch} matches the current browser process's bitness. When
     *     false (e.g. a 32-bit browser launching a 64-bit handler), host-process library paths like
     *     {@code java.library.path} are transformed so the target linker does not load
     *     wrong-bitness system libraries.
     */
    @CalledByNative
    public static String[] makePackagePaths(String arch, boolean isSameBitness) {
        PackageInfo pi =
                PackageUtils.getApplicationPackageInfo(
                        PackageManager.GET_SHARED_LIBRARY_FILES
                                | PackageManager.MATCH_UNINSTALLED_PACKAGES);

        List<String> zipPaths = new ArrayList<>(10);
        if (pi.applicationInfo.sourceDir != null) {
            zipPaths.add(pi.applicationInfo.sourceDir);
        }
        if (pi.applicationInfo.splitSourceDirs != null) {
            Collections.addAll(zipPaths, pi.applicationInfo.splitSourceDirs);
        }

        if (pi.applicationInfo.sharedLibraryFiles != null) {
            Collections.addAll(zipPaths, pi.applicationInfo.sharedLibraryFiles);
        }

        List<String> libPaths = new ArrayList<>(10);
        if (pi.applicationInfo.nativeLibraryDir != null) {
            File parent = new File(pi.applicationInfo.nativeLibraryDir).getParentFile();
            if (parent != null) {
                libPaths.add(new File(parent, arch).getPath());

                // arch is the target library's ABI name. This is the name of the library
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
        }
        for (String zip : zipPaths) {
            if (zip.endsWith(".apk")) {
                libPaths.add(zip + "!/lib/" + arch);
            }
        }
        String javaLibraryPath = System.getProperty("java.library.path");
        if (isSameBitness) {
            if (javaLibraryPath != null) {
                libPaths.add(javaLibraryPath);
            }
            libPaths.add(pi.applicationInfo.nativeLibraryDir);
        } else {
            assert arch.contains("64")
                    : "Tried calling makePackagePaths with different bitness in a 64 bit"
                            + " browser process.";
            if (javaLibraryPath != null) {
                for (String path : javaLibraryPath.split(File.pathSeparator)) {
                    if (path.endsWith("/lib")) {
                        libPaths.add(path + "64");
                    } else if (path.endsWith("/lib64")) {
                        libPaths.add(path);
                    }
                }
            }
        }

        return new String[] {
            TextUtils.join(File.pathSeparator, zipPaths),
            TextUtils.join(File.pathSeparator, libPaths)
        };
    }
}
