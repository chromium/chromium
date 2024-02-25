// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;

/** Test-implementation of MinidumpUploaderDelegate. */
class TestMinidumpUploaderDelegate implements MinidumpUploaderDelegate {
    private CrashReportingPermissionManager mPermissionManager;
    private File mCacheDir;

    public TestMinidumpUploaderDelegate(
            File cacheDir, CrashReportingPermissionManager permissionManager) {
        mCacheDir = cacheDir;
        mPermissionManager = permissionManager;
    }

    @Override
    public File getCrashParentDir() {
        return mCacheDir;
    }

    @Override
    public CrashReportingPermissionManager createCrashReportingPermissionManager() {
        return mPermissionManager;
    }

    @Override
    public void prepareToUploadMinidumps(Runnable startUploads) {
        startUploads.run();
    }

    @Override
    public void recordUploadSuccess(File minidump) {}

    @Override
    public void recordUploadFailure(File minidump) {}
}
