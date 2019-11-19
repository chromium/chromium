// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.minidump_uploader.CrashTestRule.MockCrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable.MinidumpUploadStatus;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactory;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Unittests for {@link MinidumpUploadCallable}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class MinidumpUploadCallableTest {
    @Rule
    public CrashTestRule mTestRule = new CrashTestRule();

    private static final String BOUNDARY = "TESTBOUNDARY";
    private static final String UPLOAD_CRASH_ID = "IMACRASHID";
    private static final String LOCAL_CRASH_ID = "123_log";
    private static final String LOG_FILE_NAME = "chromium_renderer-123_log.dmp224";
    private File mTestUpload;
    private File mUploadLog;
    private File mExpectedFileAfterUpload;

    /**
     * A HttpURLConnection that performs some basic checks to ensure we are uploading
     * minidumps correctly.
     */
    public static class TestHttpURLConnection extends HttpURLConnection {
        static final String DEFAULT_EXPECTED_CONTENT_TYPE =
                String.format(MinidumpUploadCallable.CONTENT_TYPE_TMPL, BOUNDARY);
        private final String mExpectedContentType;

        /**
         * The value of the "Content-Type" property if the property has been set.
         */
        private String mContentTypePropertyValue = "";

        public TestHttpURLConnection(URL url) {
            this(url, DEFAULT_EXPECTED_CONTENT_TYPE);
        }

        public TestHttpURLConnection(URL url, String contentType) {
            super(url);
            mExpectedContentType = contentType;
            Assert.assertEquals(MinidumpUploadCallable.CRASH_URL_STRING, url.toString());
        }

        @Override
        public void disconnect() {
            // Check that the "Content-Type" property has been set and the property's value.
            Assert.assertEquals(mExpectedContentType, mContentTypePropertyValue);
        }

        @Override
        public InputStream getInputStream() {
            return new ByteArrayInputStream(ApiCompatibilityUtils.getBytesUtf8(UPLOAD_CRASH_ID));
        }

        @Override
        public OutputStream getOutputStream() {
            return new ByteArrayOutputStream();
        }

        @Override
        public int getResponseCode() {
            return 200;
        }

        @Override
        public String getResponseMessage() {
            return null;
        }

        @Override
        public boolean usingProxy() {
            return false;
        }

        @Override
        public void connect() {
        }

        @Override
        public void setRequestProperty(String key, String value) {
            if (key.equals("Content-Type")) {
                mContentTypePropertyValue = value;
            }
        }
    }

    /**
     * A HttpURLConnectionFactory that performs some basic checks to ensure we are uploading
     * minidumps correctly.
     */
    public static class TestHttpURLConnectionFactory implements HttpURLConnectionFactory {
        String mContentType;

        public TestHttpURLConnectionFactory() {
            mContentType = TestHttpURLConnection.DEFAULT_EXPECTED_CONTENT_TYPE;
        }

        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            try {
                return new TestHttpURLConnection(new URL(url), mContentType);
            } catch (IOException e) {
                return null;
            }
        }
    }

    private static class ErrorCodeHttpUrlConnectionFactory implements HttpURLConnectionFactory {
        private final int mErrorCode;

        ErrorCodeHttpUrlConnectionFactory(int errorCode) {
            mErrorCode = errorCode;
        }

        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            try {
                return new TestHttpURLConnection(new URL(url)) {
                    @Override
                    public int getResponseCode() {
                        return mErrorCode;
                    }
                };
            } catch (IOException e) {
                return null;
            }
        }
    }

    private static class FailHttpURLConnectionFactory implements HttpURLConnectionFactory {
        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            Assert.fail();
            return null;
        }
    }

    /**
     * This class calls |getInstrumentation| which cannot be done in a static context.
     */
    private class MockMinidumpUploadCallable extends MinidumpUploadCallable {
        MockMinidumpUploadCallable(
                HttpURLConnectionFactory httpURLConnectionFactory,
                CrashReportingPermissionManager permManager) {
            super(mTestUpload, mUploadLog, httpURLConnectionFactory, permManager);
        }
    }

    private void createMinidumpFile() throws Exception {
        mTestUpload = new File(mTestRule.getCrashDir(), LOG_FILE_NAME);
        CrashTestRule.setUpMinidumpFile(mTestUpload, BOUNDARY);
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

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
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

        HttpURLConnectionFactory httpURLConnectionFactory = new FailHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        Assert.assertEquals(
                MinidumpUploadStatus.USER_DISABLED, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload = new File(
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

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.DISABLED_BY_SAMPLING,
                minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload = new File(
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

        HttpURLConnectionFactory httpURLConnectionFactory = new FailHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
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

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
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

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
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

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload = new File(
                mTestRule.getCrashDir(), mTestUpload.getName().replace(".forced", ".skipped"));
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

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload = new File(
                mTestRule.getCrashDir(), mTestUpload.getName().replace(".forced", ".skipped"));
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

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload = new File(
                mTestRule.getCrashDir(), mTestUpload.getName().replace(".forced", ".skipped"));
        Assert.assertFalse(expectedSkippedFileAfterUpload.exists());
        Assert.assertTrue(mExpectedFileAfterUpload.exists());
    }

    // This is a regression test for http://crbug.com/712420
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallWithInvalidMinidumpBoundary() throws Exception {
        // Include an invalid character, '[', in the test string.
        CrashTestRule.setUpMinidumpFile(mTestUpload, "--InvalidBoundaryWithSpecialCharacter--[");
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory() {
            { mContentType = ""; }
        };

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);

        Assert.assertEquals(MinidumpUploadStatus.FAILURE, minidumpUploadCallable.call().intValue());
        Assert.assertFalse(mExpectedFileAfterUpload.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallWithValidMinidumpBoundary() throws Exception {
        // Include all valid characters in the test string.
        final String boundary = "--0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        final String expectedContentType =
                String.format(MinidumpUploadCallable.CONTENT_TYPE_TMPL, boundary);
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory() {
            { mContentType = expectedContentType; }
        };

        CrashTestRule.setUpMinidumpFile(mTestUpload, boundary);

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);

        Assert.assertEquals(MinidumpUploadStatus.SUCCESS, minidumpUploadCallable.call().intValue());
        Assert.assertTrue(mExpectedFileAfterUpload.exists());
        assertValidUploadLogEntry();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testReceivingErrorCodes() {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsUserPermitted = true;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        final int[] errorCodes = {400, 401, 403, 404, 500};

        for (int n = 0; n < errorCodes.length; n++) {
            HttpURLConnectionFactory httpURLConnectionFactory =
                    new ErrorCodeHttpUrlConnectionFactory(errorCodes[n]);
            MinidumpUploadCallable minidumpUploadCallable =
                    new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
            Assert.assertEquals(
                    MinidumpUploadStatus.FAILURE, minidumpUploadCallable.call().intValue());
            // Note that mTestUpload is not renamed on failure - so we can try to upload that file
            // several times during the same test.
        }
    }

    private void assertValidUploadLogEntry() throws IOException {
        File logfile = new File(mTestRule.getCrashDir(), CrashFileManager.CRASH_DUMP_LOGFILE);
        BufferedReader input =  new BufferedReader(new FileReader(logfile));
        String line = null;
        String lastEntry = null;
        while ((line = input.readLine()) != null) {
            lastEntry = line;
        }
        input.close();

        Assert.assertNotNull("We do not have a single entry in uploads.log", lastEntry);
        String[] components = lastEntry.split(",");
        Assert.assertTrue(
                "Log entry is expected to have exactly 3 components <upload-time>,<upload-id>,<local-id>",
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

        Assert.assertEquals(uploadId, UPLOAD_CRASH_ID);
        Assert.assertEquals(localId, LOCAL_CRASH_ID);
    }
}
