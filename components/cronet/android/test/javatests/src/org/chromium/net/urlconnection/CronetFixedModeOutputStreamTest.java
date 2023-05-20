// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.net.CronetTestRule.getContext;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CompareDefaultWithCronet;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunCronetHttpURLConnection;
import org.chromium.net.NativeTestServer;
import org.chromium.net.NetworkException;
import org.chromium.net.impl.CallbackExceptionImpl;

import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpRetryException;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Tests {@code getOutputStream} when {@code setFixedLengthStreamingMode} is
 * enabled.
 * Tests annotated with {@code CompareDefaultWithCronet} will run once with the
 * default HttpURLConnection implementation and then with Cronet's
 * HttpURLConnection implementation. Tests annotated with
 * {@code OnlyRunCronetHttpURLConnection} only run Cronet's implementation.
 * See {@link CronetTestBase#runTest()} for details.
 */
@DoNotBatch(
        reason = "URL#setURLStreamHandlerFactory can be called at most once during JVM lifetime")
@RunWith(AndroidJUnit4.class)
public class CronetFixedModeOutputStreamTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();
    @Rule
    public ExpectedException thrown = ExpectedException.none();

    private CronetTestFramework mTestFramework;
    private HttpURLConnection mConnection;

    @Before
    public void setUp() throws Exception {
        mTestFramework = mTestRule.buildCronetTestFramework();
        mTestRule.enableDiskCache(mTestFramework.mBuilder);
        mTestFramework.startEngine();
        mTestRule.setStreamHandlerFactory(mTestFramework.mCronetEngine);
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @After
    public void tearDown() throws Exception {
        if (mConnection != null) {
            mConnection.disconnect();
        }
        NativeTestServer.shutdownNativeTestServer();
        mTestFramework.shutdownEngine();
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testConnectBeforeWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length);
        OutputStream out = mConnection.getOutputStream();
        mConnection.connect();
        out.write(TestUtil.UPLOAD_DATA);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection))
                .isEqualTo(TestUtil.UPLOAD_DATA_STRING);
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    // Regression test for crbug.com/687600.
    public void testZeroLengthWriteWithNoResponseBody() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setFixedLengthStreamingMode(0);
        OutputStream out = mConnection.getOutputStream();
        out.write(new byte[] {});
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testWriteAfterRequestFailed() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        mConnection.setFixedLengthStreamingMode(largeData.length);
        OutputStream out = mConnection.getOutputStream();
        out.write(largeData, 0, 10);
        NativeTestServer.shutdownNativeTestServer();
        try {
            out.write(largeData, 10, largeData.length - 10);
            mConnection.getResponseCode();
            fail();
        } catch (IOException e) {
            // Expected.
            if (!mTestRule.testingSystemHttpURLConnection()) {
                NetworkException requestException = (NetworkException) e;
                assertThat(requestException.getErrorCode())
                        .isEqualTo(NetworkException.ERROR_CONNECTION_REFUSED);
            }
        }
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testGetResponseAfterWriteFailed() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        NativeTestServer.shutdownNativeTestServer();
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Set content-length as 1 byte, so Cronet will upload once that 1 byte
        // is passed to it.
        mConnection.setFixedLengthStreamingMode(1);
        try {
            OutputStream out = mConnection.getOutputStream();
            out.write(1);
            // Forces OutputStream implementation to flush. crbug.com/653072
            out.flush();
            // System's implementation is flaky see crbug.com/653072.
            if (!mTestRule.testingSystemHttpURLConnection()) {
                fail();
            }
        } catch (IOException e) {
            if (!mTestRule.testingSystemHttpURLConnection()) {
                NetworkException requestException = (NetworkException) e;
                assertThat(requestException.getErrorCode())
                        .isEqualTo(NetworkException.ERROR_CONNECTION_REFUSED);
            }
        }
        // Make sure IOException is reported again when trying to read response
        // from the mConnection.
        try {
            mConnection.getResponseCode();
            fail();
        } catch (IOException e) {
            // Expected.
            if (!mTestRule.testingSystemHttpURLConnection()) {
                NetworkException requestException = (NetworkException) e;
                assertThat(requestException.getErrorCode())
                        .isEqualTo(NetworkException.ERROR_CONNECTION_REFUSED);
            }
        }
        // Restarting server to run the test for a second time.
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testFixedLengthStreamingModeZeroContentLength() throws Exception {
        // Check content length is set.
        URL echoLength = new URL(NativeTestServer.getEchoHeaderURL("Content-Length"));
        mConnection = (HttpURLConnection) echoLength.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setFixedLengthStreamingMode(0);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection)).isEqualTo("0");
        mConnection.disconnect();

        // Check body is empty.
        URL echoBody = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) echoBody.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setFixedLengthStreamingMode(0);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection)).isEmpty();
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testWriteLessThanContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Set a content length that's 1 byte more.
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length + 1);
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        try {
            mConnection.getResponseCode();
            fail();
        } catch (IOException e) {
            // Expected.
        }
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testWriteMoreThanContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Set a content length that's 1 byte short.
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length - 1);
        OutputStream out = mConnection.getOutputStream();
        try {
            out.write(TestUtil.UPLOAD_DATA);
            // On Lollipop, default implementation only triggers the error when reading response.
            mConnection.getInputStream();
            fail();
        } catch (IOException e) {
            // Expected.
            assertThat(e).hasMessageThat().isEqualTo("expected " + (TestUtil.UPLOAD_DATA.length - 1)
                    + " bytes but received " + TestUtil.UPLOAD_DATA.length);
        }
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testWriteMoreThanContentLengthWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Set a content length that's 1 byte short.
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length - 1);
        OutputStream out = mConnection.getOutputStream();
        for (int i = 0; i < TestUtil.UPLOAD_DATA.length - 1; i++) {
            out.write(TestUtil.UPLOAD_DATA[i]);
        }
        try {
            // Try upload an extra byte.
            out.write(TestUtil.UPLOAD_DATA[TestUtil.UPLOAD_DATA.length - 1]);
            // On Lollipop, default implementation only triggers the error when reading response.
            mConnection.getInputStream();
            fail();
        } catch (IOException e) {
            // Expected.
            String expectedVariant = "expected 0 bytes but received 1";
            String expectedVariantOnLollipop = "expected " + (TestUtil.UPLOAD_DATA.length - 1)
                    + " bytes but received " + TestUtil.UPLOAD_DATA.length;
            assertTrue(expectedVariant.equals(e.getMessage())
                    || expectedVariantOnLollipop.equals(e.getMessage()));
        }
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testFixedLengthStreamingMode() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length);
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection))
                .isEqualTo(TestUtil.UPLOAD_DATA_STRING);
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testFixedLengthStreamingModeWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length);
        OutputStream out = mConnection.getOutputStream();
        for (int i = 0; i < TestUtil.UPLOAD_DATA.length; i++) {
            // Write one byte at a time.
            out.write(TestUtil.UPLOAD_DATA[i]);
        }
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection))
                .isEqualTo(TestUtil.UPLOAD_DATA_STRING);
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testFixedLengthStreamingModeLargeData() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // largeData is 1.8 MB.
        byte[] largeData = TestUtil.getLargeData();
        mConnection.setFixedLengthStreamingMode(largeData.length);
        OutputStream out = mConnection.getOutputStream();
        int totalBytesWritten = 0;
        // Number of bytes to write each time. It is doubled each time
        // to make sure that the implementation can handle large writes.
        int bytesToWrite = 683;
        while (totalBytesWritten < largeData.length) {
            if (bytesToWrite > largeData.length - totalBytesWritten) {
                // Do not write out of bound.
                bytesToWrite = largeData.length - totalBytesWritten;
            }
            out.write(largeData, totalBytesWritten, bytesToWrite);
            totalBytesWritten += bytesToWrite;
            // About 5th iteration of this loop, bytesToWrite will be bigger than 16384.
            bytesToWrite *= 2;
        }
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testFixedLengthStreamingModeLargeDataWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        mConnection.setFixedLengthStreamingMode(largeData.length);
        OutputStream out = mConnection.getOutputStream();
        for (int i = 0; i < largeData.length; i++) {
            // Write one byte at a time.
            out.write(largeData[i]);
        }
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    @OnlyRunCronetHttpURLConnection
    public void testJavaBufferSizeLargerThanNativeBufferSize() throws Exception {
        // Set an internal buffer of size larger than the buffer size used
        // in network stack internally.
        // Normal stream uses 16384, QUIC uses 14520, and SPDY uses 16384.
        // Try two different buffer lengths. 17384 will make the last write
        // smaller than the native buffer length; 18384 will make the last write
        // bigger than the native buffer length
        // (largeData.length % 17384 = 9448, largeData.length % 18384 = 16752).
        int[] bufferLengths = new int[] {17384, 18384};
        for (int length : bufferLengths) {
            CronetFixedModeOutputStream.setDefaultBufferLengthForTesting(length);
            // Run the following three tests with this custom buffer size.
            testFixedLengthStreamingModeLargeDataWriteOneByte();
            testFixedLengthStreamingModeLargeData();
            testOneMassiveWrite();
        }
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testOneMassiveWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        mConnection.setFixedLengthStreamingMode(largeData.length);
        OutputStream out = mConnection.getOutputStream();
        // Write everything at one go, so the data is larger than the buffer
        // used in CronetFixedModeOutputStream.
        out.write(largeData);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    private static class CauseMatcher extends TypeSafeMatcher<Throwable> {
        private final Class<? extends Throwable> mType;
        private final String mExpectedMessage;

        public CauseMatcher(Class<? extends Throwable> type, String expectedMessage) {
            this.mType = type;
            this.mExpectedMessage = expectedMessage;
        }

        @Override
        protected boolean matchesSafely(Throwable item) {
            return item.getClass().isAssignableFrom(mType)
                    && item.getMessage().equals(mExpectedMessage);
        }
        @Override
        public void describeTo(Description description) {}
    }

    @Test
    @SmallTest
    @OnlyRunCronetHttpURLConnection
    public void testRewindWithCronet() throws Exception {
        assertFalse(mTestRule.testingSystemHttpURLConnection());
        // Post preserving redirect should fail.
        URL url = new URL(NativeTestServer.getRedirectToEchoBody());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length);
        thrown.expect(instanceOf(CallbackExceptionImpl.class));
        thrown.expectMessage("Exception received from UploadDataProvider");
        thrown.expectCause(
                new CauseMatcher(HttpRetryException.class, "Cannot retry streamed Http body"));
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        mConnection.getResponseCode();
    }
}
