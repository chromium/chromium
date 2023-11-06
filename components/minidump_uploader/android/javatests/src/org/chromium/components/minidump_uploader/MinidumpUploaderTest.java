// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactory;

import java.io.File;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;

/** Unittests for {@link MinidumpUploadCallable}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MinidumpUploaderTest {
    @Rule public CrashTestRule mTestRule = new CrashTestRule();
    private File mUploadTestFile;

    /* package */ static class ErrorCodeHttpURLConnectionFactory
            implements HttpURLConnectionFactory {
        private final int mErrorCode;

        ErrorCodeHttpURLConnectionFactory(int errorCode) {
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

    /* package */ static class FailHttpURLConnectionFactory implements HttpURLConnectionFactory {
        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            Assert.fail();
            return null;
        }
    }

    @Before
    public void setUp() throws IOException {
        mUploadTestFile = new File(mTestRule.getCrashDir(), "crashFile");
        CrashTestRule.setUpMinidumpFile(mUploadTestFile, MinidumpUploaderTestConstants.BOUNDARY);
    }

    @After
    public void tearDown() throws IOException {
        mUploadTestFile.delete();
    }

    // This is a regression test for http://crbug.com/712420
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallWithInvalidMinidumpBoundary() throws Exception {
        // Include an invalid character, '[', in the test string.
        final String boundary = "--InvalidBoundaryWithSpecialCharacter--[";

        CrashTestRule.setUpMinidumpFile(mUploadTestFile, boundary);

        HttpURLConnectionFactory httpURLConnectionFactory =
                new TestHttpURLConnectionFactory() {
                    {
                        mContentType = "";
                    }
                };

        MinidumpUploader minidumpUploader = new MinidumpUploader(httpURLConnectionFactory);
        MinidumpUploader.Result result = minidumpUploader.upload(mUploadTestFile);
        Assert.assertTrue(result.isFailure());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallWithValidMinidumpBoundary() throws Exception {
        // Include all valid characters in the test string.
        final String boundary = "--0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        final String expectedContentType =
                String.format(MinidumpUploader.CONTENT_TYPE_TMPL, boundary);

        CrashTestRule.setUpMinidumpFile(mUploadTestFile, boundary);

        HttpURLConnectionFactory httpURLConnectionFactory =
                new TestHttpURLConnectionFactory() {
                    {
                        mContentType = expectedContentType;
                    }
                };

        MinidumpUploader minidumpUploader = new MinidumpUploader(httpURLConnectionFactory);
        MinidumpUploader.Result result = minidumpUploader.upload(mUploadTestFile);
        Assert.assertTrue(result.isSuccess());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testReceivingErrorCodes() {
        final int[] errorCodes = {400, 401, 403, 404, 500};

        for (int n = 0; n < errorCodes.length; n++) {
            HttpURLConnectionFactory httpURLConnectionFactory =
                    new ErrorCodeHttpURLConnectionFactory(errorCodes[n]);
            MinidumpUploader minidumpUploader = new MinidumpUploader(httpURLConnectionFactory);
            MinidumpUploader.Result result = minidumpUploader.upload(mUploadTestFile);
            Assert.assertTrue(result.isUploadError());
            Assert.assertEquals(result.errorCode(), errorCodes[n]);
        }
    }
}
