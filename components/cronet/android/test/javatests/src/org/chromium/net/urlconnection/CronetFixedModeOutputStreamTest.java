// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.NativeTestServer;
import org.chromium.net.NetworkException;
import org.chromium.net.impl.CallbackExceptionImpl;

import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpRetryException;
import java.net.HttpURLConnection;
import java.net.URL;

/** Tests {@code getOutputStream} when {@code setFixedLengthStreamingMode} is enabled. */
@Batch(Batch.UNIT_TESTS)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "See crrev.com/c/4590329")
@RunWith(AndroidJUnit4.class)
public class CronetFixedModeOutputStreamTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private HttpURLConnection mConnection;

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> mTestRule.getTestFramework().enableDiskCache(builder));
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        if (mConnection != null) {
            mConnection.disconnect();
        }
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    public void testConnectBeforeWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    // Regression test for crbug.com/687600.
    public void testZeroLengthWriteWithNoResponseBody() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testWriteAfterRequestFailed() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        mConnection.setFixedLengthStreamingMode(largeData.length);
        OutputStream out = mConnection.getOutputStream();
        out.write(largeData, 0, 10);
        NativeTestServer.shutdownNativeTestServer();
        IOException e =
                assertThrows(
                        IOException.class, () -> out.write(largeData, 10, largeData.length - 10));
        // TODO(crbug.com/40286644): Consider whether we should be checking this in the first place.
        if (mTestRule.implementationUnderTest().equals(CronetImplementation.STATICALLY_LINKED)) {
            assertThat(e).isInstanceOf(NetworkException.class);
            NetworkException networkException = (NetworkException) e;
            assertThat(networkException.getErrorCode())
                    .isEqualTo(NetworkException.ERROR_CONNECTION_REFUSED);
        }
    }

    @Test
    @SmallTest
    public void testGetResponseAfterWriteFailed() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        NativeTestServer.shutdownNativeTestServer();
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Set content-length as 1 byte, so Cronet will upload once that 1 byte
        // is passed to it.
        mConnection.setFixedLengthStreamingMode(1);
        OutputStream out = mConnection.getOutputStream();
        // Forces OutputStream implementation to flush. crbug.com/653072
        IOException e = assertThrows(IOException.class, () -> out.write(1));
        // TODO(crbug.com/40286644): Consider whether we should be checking this in the first place.
        if (mTestRule.implementationUnderTest().equals(CronetImplementation.STATICALLY_LINKED)) {
            assertThat(e).isInstanceOf(NetworkException.class);
            NetworkException networkException = (NetworkException) e;
            assertThat(networkException.getErrorCode())
                    .isEqualTo(NetworkException.ERROR_CONNECTION_REFUSED);
        }
        // Make sure NetworkException is reported again when trying to read response
        // from the mConnection.
        e = assertThrows(IOException.class, mConnection::getResponseCode);
        // TODO(crbug.com/40286644): Consider whether we should be checking this in the first place.
        if (mTestRule.implementationUnderTest().equals(CronetImplementation.STATICALLY_LINKED)) {
            assertThat(e).isInstanceOf(NetworkException.class);
            NetworkException networkException = (NetworkException) e;
            assertThat(networkException.getErrorCode())
                    .isEqualTo(NetworkException.ERROR_CONNECTION_REFUSED);
        }
        // Restarting server to run the test for a second time.
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @Test
    @SmallTest
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
    public void testWriteLessThanContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Set a content length that's 1 byte more.
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length + 1);
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        assertThrows(IOException.class, mConnection::getResponseCode);
    }

    @Test
    @SmallTest
    public void testWriteMoreThanContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Set a content length that's 1 byte short.
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length - 1);
        OutputStream out = mConnection.getOutputStream();
        IOException e = assertThrows(IOException.class, () -> out.write(TestUtil.UPLOAD_DATA));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "expected "
                                + (TestUtil.UPLOAD_DATA.length - 1)
                                + " bytes but received "
                                + TestUtil.UPLOAD_DATA.length);
    }

    @Test
    @SmallTest
    public void testWriteMoreThanContentLengthWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Set a content length that's 1 byte short.
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length - 1);
        OutputStream out = mConnection.getOutputStream();
        for (int i = 0; i < TestUtil.UPLOAD_DATA.length - 1; i++) {
            out.write(TestUtil.UPLOAD_DATA[i]);
        }
        // Try upload an extra byte.
        IOException e =
                assertThrows(
                        IOException.class,
                        () -> out.write(TestUtil.UPLOAD_DATA[TestUtil.UPLOAD_DATA.length - 1]));
        assertThat(e).hasMessageThat().isEqualTo("expected 0 bytes but received 1");
    }

    @Test
    @SmallTest
    public void testFixedLengthStreamingMode() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testFixedLengthStreamingModeWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testFixedLengthStreamingModeLargeData() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testFixedLengthStreamingModeLargeDataWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testOneMassiveWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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

    @Test
    @SmallTest
    public void testRewindWithCronet() throws Exception {
        // Post preserving redirect should fail.
        URL url = new URL(NativeTestServer.getRedirectToEchoBody());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setFixedLengthStreamingMode(TestUtil.UPLOAD_DATA.length);

        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        IOException e = assertThrows(IOException.class, mConnection::getResponseCode);
        // TODO(crbug.com/40286644): Consider whether we should be checking this in the first place.
        if (mTestRule.implementationUnderTest().equals(CronetImplementation.STATICALLY_LINKED)) {
            assertThat(e).isInstanceOf(CallbackExceptionImpl.class);
        }

        assertThat(e).hasMessageThat().isEqualTo("Exception received from UploadDataProvider");
        assertThat(e).hasCauseThat().isInstanceOf(HttpRetryException.class);
        assertThat(e).hasCauseThat().hasMessageThat().isEqualTo("Cannot retry streamed Http body");
    }
}
