// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import static org.junit.Assert.assertEquals;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.task.test.PausedExecutorTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.minidump_uploader.CrashTestRule.MockCrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactory;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

/** Tests for the common MinidumpUploadJob implementation within the minidump_uploader component. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MinidumpUploadJobImplTest {
    @Rule public CrashTestRule mCrashTestRule = new CrashTestRule();
    @Rule public PausedExecutorTestRule mExecutorRule = new PausedExecutorTestRule();

    private static final String BOUNDARY = "TESTBOUNDARY";

    /** Test to ensure the minidump uploading mechanism allows the expected number of upload retries. */
    @Test
    public void testRetryCountRespected() throws IOException {
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsUserPermitted = true;
                        mIsNetworkAvailable = false; // Will cause us to fail uploads
                        mIsEnabledForTests = false;
                    }
                };

        File firstFile = createMinidumpFileInCrashDir("1_abc.dmp0.try0");

        for (int i = 0; i < MinidumpUploadJobImpl.MAX_UPLOAD_TRIES_ALLOWED; ++i) {
            uploadMinidumpsSync(
                    new TestMinidumpUploadJobImpl(
                            mCrashTestRule.getExistingCacheDir(), permManager),
                    i + 1 < MinidumpUploadJobImpl.MAX_UPLOAD_TRIES_ALLOWED);
        }
    }

    /**
     * Test to ensure the minidump uploading mechanism behaves as expected when we fail to upload
     * minidumps.
     */
    @Test
    public void testFailUploadingMinidumps() throws IOException {
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsUserPermitted = true;
                        mIsNetworkAvailable = false; // Will cause us to fail uploads
                        mIsEnabledForTests = false;
                    }
                };
        MinidumpUploadJob minidumpUploadJob =
                new TestMinidumpUploadJobImpl(mCrashTestRule.getExistingCacheDir(), permManager);

        File firstFile = createMinidumpFileInCrashDir("1_abc.dmp0.try0");
        File secondFile = createMinidumpFileInCrashDir("12_abc.dmp0.try0");
        String triesBelowMaxString = ".try" + (MinidumpUploadJobImpl.MAX_UPLOAD_TRIES_ALLOWED - 1);
        String maxTriesString = ".try" + MinidumpUploadJobImpl.MAX_UPLOAD_TRIES_ALLOWED;
        File justBelowMaxTriesFile =
                createMinidumpFileInCrashDir("belowmaxtries.dmp0" + triesBelowMaxString);
        File maxTriesFile = createMinidumpFileInCrashDir("maxtries.dmp0" + maxTriesString);

        File expectedFirstFile = new File(mCrashTestRule.getCrashDir(), "1_abc.dmp0.try1");
        File expectedSecondFile = new File(mCrashTestRule.getCrashDir(), "12_abc.dmp0.try1");
        File expectedJustBelowMaxTriesFile =
                new File(
                        mCrashTestRule.getCrashDir(),
                        justBelowMaxTriesFile
                                .getName()
                                .replace(triesBelowMaxString, maxTriesString));

        uploadMinidumpsSync(minidumpUploadJob, /* expectReschedule= */ true);
        Assert.assertFalse(firstFile.exists());
        Assert.assertFalse(secondFile.exists());
        Assert.assertFalse(justBelowMaxTriesFile.exists());
        Assert.assertTrue(expectedFirstFile.exists());
        Assert.assertTrue(expectedSecondFile.exists());
        Assert.assertTrue(expectedJustBelowMaxTriesFile.exists());
        // This file should have been left untouched.
        Assert.assertTrue(maxTriesFile.exists());
    }

    @Test
    public void testFailingThenPassingUpload() throws IOException {
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsEnabledForTests = true;
                    }
                };
        List<MinidumpUploadCallableCreator> callables = new ArrayList<>();
        callables.add(
                new MinidumpUploadCallableCreator() {
                    @Override
                    public MinidumpUploadCallable createCallable(File minidumpFile, File logfile) {
                        return new MinidumpUploadCallable(
                                minidumpFile,
                                logfile,
                                new MinidumpUploader(new FailingHttpUrlConnectionFactory()),
                                permManager);
                    }
                });
        callables.add(
                new MinidumpUploadCallableCreator() {
                    @Override
                    public MinidumpUploadCallable createCallable(File minidumpFile, File logfile) {
                        return new MinidumpUploadCallable(
                                minidumpFile,
                                logfile,
                                new MinidumpUploader(new TestHttpURLConnectionFactory()),
                                permManager);
                    }
                });
        MinidumpUploadJob minidumpUploadJob =
                createCallableListMinidumpUploadJob(
                        callables, permManager.isUsageAndCrashReportingPermitted());

        File firstFile = createMinidumpFileInCrashDir("firstFile.dmp0.try0");
        File secondFile = createMinidumpFileInCrashDir("secondFile.dmp0.try0");

        uploadMinidumpsSync(minidumpUploadJob, /* expectReschedule= */ true);
        Assert.assertFalse(firstFile.exists());
        Assert.assertFalse(secondFile.exists());
        File expectedSecondFile;
        // Not sure which minidump will fail and which will succeed, so just ensure one was uploaded
        // and the other one failed.
        if (new File(mCrashTestRule.getCrashDir(), "firstFile.dmp0.try1").exists()) {
            expectedSecondFile = new File(mCrashTestRule.getCrashDir(), "secondFile.up0.try0");
        } else {
            File uploadedFirstFile = new File(mCrashTestRule.getCrashDir(), "firstFile.up0.try0");
            Assert.assertTrue(uploadedFirstFile.exists());
            expectedSecondFile = new File(mCrashTestRule.getCrashDir(), "secondFile.dmp0.try1");
        }
        Assert.assertTrue(expectedSecondFile.exists());
    }

    /**
     * Prior to M60, the ".try0" suffix was optional; however now it is not. This test verifies that
     * the code rejects minidumps that lack this suffix.
     */
    @Test
    public void testInvalidMinidumpNameGeneratesNoUploads() throws IOException {
        MinidumpUploadJob minidumpUploadJob =
                new ExpectNoUploadsMinidumpUploadJobImpl(mCrashTestRule.getExistingCacheDir());

        // Note the omitted ".try0" suffix.
        File fileUsingLegacyNamingScheme = createMinidumpFileInCrashDir("1_abc.dmp0");

        uploadMinidumpsSync(minidumpUploadJob, /* expectReschedule= */ false);

        // The file should not have been touched, nor should any successful upload files have
        // appeared.
        Assert.assertTrue(fileUsingLegacyNamingScheme.exists());
        Assert.assertFalse(new File(mCrashTestRule.getCrashDir(), "1_abc.up0").exists());
        Assert.assertFalse(new File(mCrashTestRule.getCrashDir(), "1_abc.up0.try0").exists());
    }

    @Test
    public void testCancelMinidumpUploadsFailedUpload() throws IOException {
        doUploadTest(false, true);
    }

    @Test
    public void testCancelingWontCancelFirstUpload() throws IOException {
        doUploadTest(true, true);
    }

    @Test
    public void testFailedUploadCausesReschedule() throws IOException {
        doUploadTest(false, false);
    }

    @Test
    public void testNormalUpload() throws IOException {
        doUploadTest(true, false);
    }

    private void doUploadTest(boolean successfulUpload, boolean shouldCancel) throws IOException {
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsEnabledForTests = true;
                    }
                };
        FakeMinidumpUploadJobImpl minidumpUploadJob =
                new FakeMinidumpUploadJobImpl(
                        mCrashTestRule.getExistingCacheDir(),
                        permManager,
                        successfulUpload,
                        shouldCancel);

        File firstFile = createMinidumpFileInCrashDir("123_abc.dmp0.try0");

        ArrayList<Boolean> results = new ArrayList<>();
        minidumpUploadJob.uploadAllMinidumps(results::add);
        // Wait until our job finished.
        mExecutorRule.runAllBackgroundAndUi();
        Assert.assertTrue(minidumpUploadJob.mWasRun);
        Assert.assertEquals(shouldCancel ? true : null, minidumpUploadJob.mCancelReturnValue);
        Assert.assertEquals(shouldCancel ? List.of() : List.of(!successfulUpload), results);

        File expectedFirstUploadFile = new File(mCrashTestRule.getCrashDir(), "123_abc.up0.try0");
        File expectedFirstRetryFile = new File(mCrashTestRule.getCrashDir(), "123_abc.dmp0.try1");
        if (successfulUpload) {
            // When the upload succeeds we expect the file to be renamed.
            Assert.assertFalse(firstFile.exists());
            Assert.assertTrue(expectedFirstUploadFile.exists());
            Assert.assertFalse(expectedFirstRetryFile.exists());
        } else {
            // When the upload fails we won't change the minidump at all.
            Assert.assertEquals(shouldCancel, firstFile.exists());
            Assert.assertFalse(expectedFirstUploadFile.exists());
            Assert.assertEquals(!shouldCancel, expectedFirstRetryFile.exists());
        }
    }

    private void uploadMinidumpsSync(
            MinidumpUploadJob minidumpUploadJob, boolean expectReschedule) {
        ArrayList<Boolean> wasRescheduled = new ArrayList<>();
        minidumpUploadJob.uploadAllMinidumps(wasRescheduled::add);
        mExecutorRule.runAllBackgroundAndUi();
        assertEquals(List.of(expectReschedule), wasRescheduled);
    }

    private interface MinidumpUploadCallableCreator {
        MinidumpUploadCallable createCallable(File minidumpFile, File logfile);
    }

    private MinidumpUploadJobImpl createCallableListMinidumpUploadJob(
            final List<MinidumpUploadCallableCreator> callables, final boolean userPermitted) {
        return new TestMinidumpUploadJobImpl(mCrashTestRule.getExistingCacheDir(), null) {
            private int mIndex;

            @Override
            public MinidumpUploadCallable createMinidumpUploadCallable(
                    File minidumpFile, File logfile) {
                if (mIndex >= callables.size()) {
                    Assert.fail("Should not create callable number " + mIndex);
                }
                return callables.get(mIndex++).createCallable(minidumpFile, logfile);
            }
        };
    }

    private static class ExpectNoUploadsMinidumpUploadJobImpl extends MinidumpUploadJobImpl {
        public ExpectNoUploadsMinidumpUploadJobImpl(File cacheDir) {
            super(
                    new TestMinidumpUploaderDelegate(
                            cacheDir,
                            new MockCrashReportingPermissionManager() {
                                {
                                    mIsEnabledForTests = true;
                                }
                            }));
        }

        @Override
        public CrashFileManager createCrashFileManager(File crashDir) {
            return new CrashFileManager(crashDir) {
                @Override
                public void cleanOutAllNonFreshMinidumpFiles() {}
            };
        }

        @Override
        public MinidumpUploadCallable createMinidumpUploadCallable(
                File minidumpFile, File logfile) {
            Assert.fail("No minidumps upload attempts should be initiated by this uploader.");
            return null;
        }
    }

    /** Subclass that calls cancelUpload() after network request has started. */
    private static class FakeMinidumpUploadJobImpl extends TestMinidumpUploadJobImpl {
        private final boolean mSuccessfulUpload;
        private final boolean mShouldCancel;
        public boolean mWasRun;
        public Boolean mCancelReturnValue;

        public FakeMinidumpUploadJobImpl(
                File cacheDir,
                CrashReportingPermissionManager permissionManager,
                boolean successfulUpload,
                boolean shouldCancel) {
            super(cacheDir, permissionManager);
            mSuccessfulUpload = successfulUpload;
            mShouldCancel = shouldCancel;
        }

        @Override
        public MinidumpUploadCallable createMinidumpUploadCallable(
                File minidumpFile, File logfile) {
            Runnable hook =
                    () -> {
                        mWasRun = true;
                        if (mShouldCancel) {
                            mCancelReturnValue = cancelUploads();
                        }
                    };
            return new MinidumpUploadCallable(
                    minidumpFile,
                    logfile,
                    new MinidumpUploader(new FakeHttpUrlConnectionFactory(mSuccessfulUpload, hook)),
                    mDelegate.createCrashReportingPermissionManager());
        }
    }

    private static class FakeHttpUrlConnectionFactory implements HttpURLConnectionFactory {
        private final boolean mSucceed;
        private final Runnable mPrenetworkHook;

        private class FakeOutputStream extends OutputStream {
            @Override
            public void write(int b) throws IOException {
                if (mPrenetworkHook != null) {
                    mPrenetworkHook.run();
                }
                if (!mSucceed) {
                    throw new IOException();
                }
            }
        }

        public FakeHttpUrlConnectionFactory(boolean succeed, Runnable prenetworkHook) {
            mSucceed = succeed;
            mPrenetworkHook = prenetworkHook;
        }

        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            try {
                return new TestHttpURLConnection(new URL(url)) {
                    @Override
                    public OutputStream getOutputStream() {
                        return new FakeOutputStream();
                    }
                };
            } catch (MalformedURLException e) {
                return null;
            }
        }
    }

    private static class FailingHttpUrlConnectionFactory implements HttpURLConnectionFactory {
        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            return null;
        }
    }

    private File createMinidumpFileInCrashDir(String name) throws IOException {
        File minidumpFile = new File(mCrashTestRule.getCrashDir(), name);
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY);
        return minidumpFile;
    }
}
