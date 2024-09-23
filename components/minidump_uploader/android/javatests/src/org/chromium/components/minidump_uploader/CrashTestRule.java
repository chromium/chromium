// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

/** TestRule for Crash upload related tests. */
public class CrashTestRule implements TestRule {
    private static final String TAG = "CrashTestRule";

    private File mCrashDir;
    private File mCacheDir;

    @Override
    public Statement apply(final Statement base, final Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                base.evaluate();
                tearDown();
            }
        };
    }

    public File getCrashDir() {
        return mCrashDir;
    }

    public File getCacheDir() {
        return mCacheDir;
    }

    private void setUp() throws Exception {
        if (mCacheDir == null) {
            mCacheDir = getExistingCacheDir();
        }
        if (mCrashDir == null) {
            mCrashDir = new File(mCacheDir, CrashFileManager.CRASH_DUMP_DIR);
        }
        if (!mCrashDir.isDirectory() && !mCrashDir.mkdir()) {
            throw new Exception("Unable to create directory: " + mCrashDir.getAbsolutePath());
        }
    }

    /**
     * Returns the cache directory where we should store minidumps.
     * Can be overriden by sub-classes to allow for use with different cache directories.
     */
    public File getExistingCacheDir() {
        return ContextUtils.getApplicationContext().getCacheDir();
    }

    private void tearDown() {
        File[] crashFiles = mCrashDir.listFiles();
        if (crashFiles == null) {
            return;
        }

        for (File crashFile : crashFiles) {
            if (!crashFile.delete()) {
                Log.e(TAG, "Unable to delete: " + crashFile.getAbsolutePath());
            }
        }
        if (!mCrashDir.delete()) {
            Log.e(TAG, "Unable to delete: " + mCrashDir.getAbsolutePath());
        }
    }

    public static void setUpMinidumpFile(File file, String boundary) throws IOException {
        setUpMinidumpFile(file, boundary, null);
    }

    public static void setUpMinidumpFile(File file, String boundary, String processType)
            throws IOException {
        PrintWriter minidumpWriter = null;
        try {
            minidumpWriter = new PrintWriter(new FileWriter(file));
            minidumpWriter.println("--" + boundary);
            minidumpWriter.println("Content-Disposition: form-data; name=\"prod\"");
            minidumpWriter.println();
            minidumpWriter.println("Chrome_Android");
            minidumpWriter.println("--" + boundary);
            minidumpWriter.println("Content-Disposition: form-data; name=\"ver\"");
            minidumpWriter.println();
            minidumpWriter.println("1");
            if (processType != null) {
                minidumpWriter.println("Content-Disposition: form-data; name=\"ptype\"");
                minidumpWriter.println();
                minidumpWriter.println(processType);
            }
            minidumpWriter.println(boundary + "--");
            minidumpWriter.flush();
        } finally {
            if (minidumpWriter != null) {
                minidumpWriter.close();
            }
        }
    }

    /**
     * A utility instantiation of CrashReportingPermissionManager providing a compact way of
     * overriding different permission settings.
     */
    public static class MockCrashReportingPermissionManager
            implements CrashReportingPermissionManager {
        protected boolean mIsInSample;
        protected boolean mIsUserPermitted;
        protected boolean mIsNetworkAvailable;
        protected boolean mIsEnabledForTests;

        public MockCrashReportingPermissionManager() {}

        @Override
        public boolean isClientInSampleForCrashes() {
            return mIsInSample;
        }

        @Override
        public boolean isNetworkAvailableForCrashUploads() {
            return mIsNetworkAvailable;
        }

        @Override
        public boolean isUsageAndCrashReportingPermittedByPolicy() {
            return true;
        }

        @Override
        public boolean isUsageAndCrashReportingPermittedByUser() {
            return mIsUserPermitted;
        }

        @Override
        public boolean isUploadEnabledForTests() {
            return mIsEnabledForTests;
        }
    }
}
