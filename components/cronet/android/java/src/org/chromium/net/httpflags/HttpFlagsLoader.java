// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * Utilities for loading HTTP flags.
 *
 * <p>HTTP flags are a generic mechanism by which the host system (i.e. the Android system image)
 * can provide values for a variety of configuration knobs to alter the behavior of the HTTP client
 * stack. The idea is that the host system can use some kind of centralized configuration mechanism
 * to remotely push changes to these settings while collecting data on the results. This in turn
 * enables A/B experiments, progressive configuration rollouts, etc.
 *
 * <p>Currently, the interface with the host system is defined as follows:
 * <ol>
 * <li>The Android system image must provide an Android app that exposes a service matching the
 *     {@link #FLAGS_FILE_PROVIDER_INTENT_ACTION} action.
 * <li>That Android app must expose a directory named after {@link #FLAGS_FILE_DIR_NAME} under the
 *     app's {@link ApplicationInfo#deviceProtectedDataDir}.
 * <li>That directory must contain a file named after {@link #FLAGS_FILE_NAME} that must be readable
 *     by the process running {@link #load}.
 * <li>The flag values are obtained from the contents of that file. The format is a binary proto
 *     that can be read through {@link Flags#parseDelimitedFrom} - see `flags.proto` for details.
 * </ol>
 *
 * @see HttpFlagsInterceptor
 */
public final class HttpFlagsLoader {
    private HttpFlagsLoader() {}

    @VisibleForTesting
    static final String FLAGS_FILE_PROVIDER_INTENT_ACTION = "android.net.http.FLAGS_FILE_PROVIDER";

    @VisibleForTesting static final String FLAGS_FILE_DIR_NAME = "app_httpflags";
    @VisibleForTesting static final String FLAGS_FILE_NAME = "flags.binarypb";

    private static final String TAG = "HttpFlagsLoader";

    /**
     * Locates and loads the HTTP flags file from the host system.
     *
     * Note that this is an expensive call.
     *
     * @return The contents of the flags file, or null if the flags file could not be loaded for any
     * reason. In the latter case, the callee will take care of logging the failure.
     *
     * @see ResolvedFlags
     */
    @Nullable
    public static Flags load(Context context) {
        try {
            ApplicationInfo providerApplicationInfo = getProviderApplicationInfo(context);
            if (providerApplicationInfo == null) return null;
            Log.d(
                    TAG,
                    "Found application exporting HTTP flags: %s",
                    providerApplicationInfo.packageName);

            File flagsFile = getFlagsFileFromProvider(context, providerApplicationInfo);
            Log.d(TAG, "HTTP flags file path: %s", flagsFile.getAbsolutePath());

            Flags flags = loadFlagsFile(flagsFile);
            if (flags == null) return null;
            Log.d(TAG, "Successfully loaded HTTP flags: %s", flags);

            return flags;
        } catch (RuntimeException exception) {
            Log.i(TAG, "Unable to load HTTP flags file", exception);
            return null;
        }
    }

    @Nullable
    private static ApplicationInfo getProviderApplicationInfo(Context context) {
        ResolveInfo resolveInfo =
                context.getPackageManager()
                        .resolveService(
                                new Intent(FLAGS_FILE_PROVIDER_INTENT_ACTION),
                                // Make sure we only read flags files that are written by a package
                                // from the system image. This prevents random third-party apps
                                // from being able to inject flags into other apps, which would be
                                // a security risk.
                                PackageManager.MATCH_SYSTEM_ONLY);
        if (resolveInfo == null) {
            Log.i(
                    TAG,
                    "Unable to resolve the HTTP flags file provider package. This is expected if "
                            + "the host system is not set up to provide HTTP flags.");
            return null;
        }

        return resolveInfo.serviceInfo.applicationInfo;
    }

    private static File getFlagsFileFromProvider(
            Context context, ApplicationInfo providerApplicationInfo) {
        return new File(
                new File(
                        new File(
                                Build.VERSION.SDK_INT >= 24
                                        ? providerApplicationInfo.deviceProtectedDataDir
                                        : providerApplicationInfo.dataDir),
                        FLAGS_FILE_DIR_NAME),
                FLAGS_FILE_NAME);
    }

    @Nullable
    private static Flags loadFlagsFile(File file) {
        try (FileInputStream fileInputStream = new FileInputStream(file)) {
            return Flags.parseDelimitedFrom(fileInputStream);
        } catch (FileNotFoundException exception) {
            Log.i(
                    TAG,
                    "HTTP flags file `%s` is missing. This is expected if HTTP flags functionality "
                            + "is currently disabled in the host system.",
                    file.getPath());
            return null;
        } catch (IOException exception) {
            throw new RuntimeException("Unable to read HTTP flags file", exception);
        }
    }
}
