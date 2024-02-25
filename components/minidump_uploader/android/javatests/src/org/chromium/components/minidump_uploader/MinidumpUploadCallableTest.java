// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.minidump_uploader.CrashTestRule.MockCrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable.MinidumpUploadStatus;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

/** Unittests for {@link MinidumpUploadCallable}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MinidumpUploadCallableTest {
    private static final String LOCAL_CRASH_ID = "123_log";
    private static final String LOG_FILE_NAME = "chromium_renderer-123_log.dmp224";

    @Rule public CrashTestRule mTestRule = new CrashTestRule();

    private File mTestUpload;
    private File mUploadLog;
    private File mExpectedFileAfterUpload;

    private static class MockMinidumpUploader extends MinidumpUploader {
        private Result mMockResult;

        public static MinidumpUploader returnsSuccess() {
            return new MockMinidumpUploader(
                    Result.success(MinidumpUploaderTestConstants.UPLOAD_CRASH_ID));
        }

        public static MinidumpUploader returnsFailure(String message) {
            return new MockMinidumpUploader(Result.failure(message));
        }

        public static MinidumpUploader returnsUploadError(int status, String message) {
            return new MockMinidumpUploader(Result.uploadError(status, message));
        }

        private MockMinidumpUploader(Result mockResult) {
            super(null);
            mMockResult = mockResult;
        }

        @Override
        public Result upload(File fileToUpload) {
            return mMockResult;
        }
    }

    private void createMinidumpFile() throws Exception {
        mTestUpload = new File(mTestRule.getCrashDir(), LOG_FILE_NAME);
        CrashTestRule.setUpMinidumpFile(mTestUpload, MinidumpUploaderTestConstants.BOUNDARY);
    }

    private void setForcedUpload() {
        File renamed =
                new File(mTestRule.getCrashDir(), mTestUpload.getName().replace(".dmp", ".forced"));
        mTestUpload.renameTo(renamed);
        // Update the filename that tests will refer to.
        mTestUpload = renamed;
    }

    @Before
    public void setUp() throws Exception {
        mUploadLog = new File(mTestRule.getCrashDir(), CrashFileManager.CRASH_DUMP_LOGFILE);
        // Delete all logs from previous runs if possible.
        mUploadLog.delete();

        // Any created files will be cleaned up as part of CrashTestRule::tearDown().
        createMinidumpFile();
        mExpectedFileAfterUpload =
                new File(mTestRule.getCrashDir(), mTestUpload.getName().replace(".dmp", ".up"));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testSuccessfulUpload() throws Exception {
        final CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsEnabledForTests = true;
                    }
                };
        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsSuccess(),
                        testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());
        Assert.assertTrue(mExpectedFileAfterUpload.exists());
        assertValidUploadLogEntry();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testFailedUploadLocalError() throws Exception {
        final CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsEnabledForTests = true;
                    }
                };
        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsFailure("Failed"),
                        testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.FAILURE, minidumpUploadCallable.call().intValue());
        Assert.assertFalse(mExpectedFileAfterUpload.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testFailedUploadRemoteError() throws Exception {
        final CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsEnabledForTests = true;
                    }
                };
        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsUploadError(404, "Not Found"),
                        testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.FAILURE, minidumpUploadCallable.call().intValue());
        Assert.assertFalse(mExpectedFileAfterUpload.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallWhenCurrentlyPermitted() throws Exception {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsUserPermitted = true;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsSuccess(),
                        testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());
        Assert.assertTrue(mExpectedFileAfterUpload.exists());
        assertValidUploadLogEntry();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallNotPermittedByUser() {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsUserPermitted = false;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsSuccess(),
                        testPermManager);
        Assert.assertEquals(
                MinidumpUploadStatus.USER_DISABLED, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload =
                new File(
                        mTestRule.getCrashDir(), mTestUpload.getName().replace(".dmp", ".skipped"));
        Assert.assertTrue(expectedSkippedFileAfterUpload.exists());
        Assert.assertFalse(mExpectedFileAfterUpload.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallPermittedButNotInSample() {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = false;
                        mIsUserPermitted = true;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsSuccess(),
                        testPermManager);
        Assert.assertEquals(
                MinidumpUploadStatus.DISABLED_BY_SAMPLING,
                minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload =
                new File(
                        mTestRule.getCrashDir(), mTestUpload.getName().replace(".dmp", ".skipped"));
        Assert.assertTrue(expectedSkippedFileAfterUpload.exists());
        Assert.assertFalse(mExpectedFileAfterUpload.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallPermittedButNotUnderCurrentCircumstances() {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsUserPermitted = true;
                        mIsNetworkAvailable = false;
                        mIsEnabledForTests = false;
                    }
                };

        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsSuccess(),
                        testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.FAILURE, minidumpUploadCallable.call().intValue());
        Assert.assertFalse(mExpectedFileAfterUpload.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCrashUploadEnabledForTestsDespiteConstraints() throws Exception {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsUserPermitted = false;
                        mIsNetworkAvailable = false;
                        mIsEnabledForTests = true;
                    }
                };

        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsSuccess(),
                        testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());
        Assert.assertTrue(mExpectedFileAfterUpload.exists());
        assertValidUploadLogEntry();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallWhenCurrentlyPermitted_ForcedUpload() throws Exception {
        setForcedUpload();
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsUserPermitted = true;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsSuccess(),
                        testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());
        Assert.assertTrue(mExpectedFileAfterUpload.exists());
        assertValidUploadLogEntry();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallNotPermittedByUser_ForcedUpload() {
        setForcedUpload();
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsUserPermitted = false;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsSuccess(),
                        testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload =
                new File(
                        mTestRule.getCrashDir(),
                        mTestUpload.getName().replace(".forced", ".skipped"));
        Assert.assertFalse(expectedSkippedFileAfterUpload.exists());
        Assert.assertTrue(mExpectedFileAfterUpload.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallPermittedButNotInSample_ForcedUpload() {
        setForcedUpload();
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = false;
                        mIsUserPermitted = true;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsSuccess(),
                        testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload =
                new File(
                        mTestRule.getCrashDir(),
                        mTestUpload.getName().replace(".forced", ".skipped"));
        Assert.assertFalse(expectedSkippedFileAfterUpload.exists());
        Assert.assertTrue(mExpectedFileAfterUpload.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallPermittedButNotUnderCurrentCircumstances_ForcedUpload() {
        setForcedUpload();
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsUserPermitted = true;
                        mIsNetworkAvailable = false;
                        mIsEnabledForTests = false;
                    }
                };

        MinidumpUploadCallable minidumpUploadCallable =
                new MinidumpUploadCallable(
                        mTestUpload,
                        mUploadLog,
                        MockMinidumpUploader.returnsSuccess(),
                        testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload =
                new File(
                        mTestRule.getCrashDir(),
                        mTestUpload.getName().replace(".forced", ".skipped"));
        Assert.assertFalse(expectedSkippedFileAfterUpload.exists());
        Assert.assertTrue(mExpectedFileAfterUpload.exists());
    }

    private void assertValidUploadLogEntry() throws IOException {
        File logfile = new File(mTestRule.getCrashDir(), CrashFileManager.CRASH_DUMP_LOGFILE);
        BufferedReader input = new BufferedReader(new FileReader(logfile));
        String line = null;
        String lastEntry = null;
        while ((line = input.readLine()) != null) {
            lastEntry = line;
        }
        input.close();

        Assert.assertNotNull("We do not have a single entry in uploads.log", lastEntry);
        String[] components = lastEntry.split(",");
        Assert.assertTrue(
                "Log entry is expected to have exactly 3 components"
                        + " <upload-time>,<upload-id>,<local-id>",
                components.length == 3);

        String uploadTimeString = components[0];
        String uploadId = components[1];
        String localId = components[2];

        long time = Long.parseLong(uploadTimeString);
        long now = System.currentTimeMillis() / 1000; // Timestamp was in seconds.

        // Sanity check on the time stamp (within an hour).
        // Chances are the write and the check should have less than 1 second in between.
        Assert.assertTrue(time <= now);
        Assert.assertTrue(time > now - 60 * 60);

        Assert.assertEquals(uploadId, MinidumpUploaderTestConstants.UPLOAD_CRASH_ID);
        Assert.assertEquals(localId, LOCAL_CRASH_ID);
    }
}
