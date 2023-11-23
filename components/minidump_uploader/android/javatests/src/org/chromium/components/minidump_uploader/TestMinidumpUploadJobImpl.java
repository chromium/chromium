// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;

/** Test-implementation of MinidumpUploadJobImpl. */
public class TestMinidumpUploadJobImpl extends MinidumpUploadJobImpl {
    public TestMinidumpUploadJobImpl(
            File cacheDir, CrashReportingPermissionManager permissionManager) {
        super(new TestMinidumpUploaderDelegate(cacheDir, permissionManager));
    }

    public TestMinidumpUploadJobImpl(MinidumpUploaderDelegate delegate) {
        super(delegate);
    }

    @Override
    public CrashFileManager createCrashFileManager(File crashDir) {
        return new CrashFileManager(crashDir) {
            @Override
            public void cleanOutAllNonFreshMinidumpFiles() {}
        };
    }

    @Override
    public MinidumpUploadCallable createMinidumpUploadCallable(File minidumpFile, File logfile) {
        return new MinidumpUploadCallable(
                minidumpFile,
                logfile,
                new MinidumpUploader(new TestHttpURLConnectionFactory()),
                mDelegate.createCrashReportingPermissionManager());
    }
}
