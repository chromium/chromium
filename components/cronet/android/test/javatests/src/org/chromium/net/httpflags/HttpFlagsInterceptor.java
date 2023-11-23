// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.os.Build;

import androidx.annotation.Nullable;

import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.net.ContextInterceptor;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.UUID;

/**
 * A {@link ContextInterceptor} that makes the intercepted Context advertise the presence (or
 * absence) of an HTTP flags file.
 *
 * @see org.chromium.net.httpflags.HttpFlagsLoader
 */
public final class HttpFlagsInterceptor implements ContextInterceptor, AutoCloseable {
    private static final String FLAGS_PROVIDER_PACKAGE_NAME =
            "org.chromium.net.httpflags.HttpFlagsInterceptor.FAKE_PROVIDER_PACKAGE";

    @Nullable private final Flags mFlagsFileContents;
    private File mDataDir;

    /** @param flagsFileContents the contents of the flags file, or null to simulate a missing file. */
    public HttpFlagsInterceptor(@Nullable Flags flagsFileContents) {
        mFlagsFileContents = flagsFileContents;
    }

    @Override
    public Context interceptContext(Context context) {
        return new HttpFlagsContextWrapper(context);
    }

    private final class HttpFlagsContextWrapper extends ContextWrapper {
        HttpFlagsContextWrapper(Context context) {
            super(context);
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public ResolveInfo resolveService(Intent intent, int flags) {
                    if (!intent.getAction()
                            .equals(HttpFlagsLoader.FLAGS_FILE_PROVIDER_INTENT_ACTION)) {
                        return super.resolveService(intent, flags);
                    }

                    assertThat(flags).isEqualTo(MATCH_SYSTEM_ONLY);

                    if (mFlagsFileContents == null) return null;
                    createFlagsFile(getBaseContext());

                    ApplicationInfo applicationInfo = new ApplicationInfo();
                    applicationInfo.packageName = FLAGS_PROVIDER_PACKAGE_NAME;
                    if (Build.VERSION.SDK_INT >= 24) {
                        applicationInfo.deviceProtectedDataDir = mDataDir.getAbsolutePath();
                    } else {
                        applicationInfo.dataDir = mDataDir.getAbsolutePath();
                    }

                    ResolveInfo resolveInfo = new ResolveInfo();
                    resolveInfo.serviceInfo = new ServiceInfo();
                    resolveInfo.serviceInfo.applicationInfo = applicationInfo;
                    return resolveInfo;
                }
            };
        }
    }

    private void createFlagsFile(Context context) {
        if (mDataDir != null) return;
        mDataDir =
                context.getDir(
                        "org.chromium.net.httpflags.FakeFlagsFileDataDir."
                                // Ensure different instances can't interfere with each other (e.g.
                                // when running multiple tests).
                                + UUID.randomUUID(),
                        Context.MODE_PRIVATE);

        File flagsFile = getFlagsFile();
        if (!flagsFile.getParentFile().mkdir()) {
            throw new RuntimeException("Unable to create flags dir");
        }
        try {
            if (!flagsFile.createNewFile()) throw new RuntimeException("File already exists");
            try (final FileOutputStream fileOutputStream = new FileOutputStream(flagsFile)) {
                mFlagsFileContents.writeDelimitedTo(fileOutputStream);
            }
        } catch (RuntimeException | IOException exception) {
            throw new RuntimeException(
                    "Failed to write fake HTTP flags file " + flagsFile, exception);
        }
    }

    @Override
    public void close() {
        if (mDataDir == null) return;

        File flagsFile = getFlagsFile();
        if (!flagsFile.delete()) {
            throw new RuntimeException("Failed to delete fake HTTP flags file " + flagsFile);
        }
        File flagsDir = flagsFile.getParentFile();
        if (!flagsDir.delete()) {
            throw new RuntimeException("Failed to delete fake HTTP flags dir " + flagsDir);
        }
        if (!mDataDir.delete()) {
            throw new RuntimeException("Failed to delete fake HTTP flags data dir " + mDataDir);
        }
        mDataDir = null;
    }

    private File getFlagsFile() {
        return new File(
                new File(mDataDir, HttpFlagsLoader.FLAGS_FILE_DIR_NAME),
                HttpFlagsLoader.FLAGS_FILE_NAME);
    }
}
