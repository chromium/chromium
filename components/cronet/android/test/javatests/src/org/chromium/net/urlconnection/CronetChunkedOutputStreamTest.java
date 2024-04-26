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

import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.ProtocolException;
import java.net.URL;

/** Tests {@code getOutputStream} when {@code setChunkedStreamingMode} is enabled. */
@Batch(Batch.UNIT_TESTS)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "See crrev.com/c/4590329")
@RunWith(AndroidJUnit4.class)
public class CronetChunkedOutputStreamTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    private static final String UPLOAD_DATA_STRING = "Nifty upload data!";
    private static final byte[] UPLOAD_DATA = UPLOAD_DATA_STRING.getBytes();
    private static final int REPEAT_COUNT = 100000;

    private HttpURLConnection mConnection;

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().getEngine();
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
    public void testGetOutputStreamAfterConnectionMade() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThrows(ProtocolException.class, mConnection::getOutputStream);
    }

    @Test
    @SmallTest
    public void testWriteAfterReadingResponse() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        OutputStream out = mConnection.getOutputStream();
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThrows(IOException.class, () -> out.write(UPLOAD_DATA));
    }

    @Test
    @SmallTest
    public void testWriteAfterRequestFailed() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        OutputStream out = mConnection.getOutputStream();
        out.write(UPLOAD_DATA);
        NativeTestServer.shutdownNativeTestServer();
        IOException e = assertThrows(IOException.class, () -> out.write(TestUtil.getLargeData()));
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
        // Set 1 byte as chunk size so internally Cronet will try upload when
        // 1 byte is filled.
        mConnection.setChunkedStreamingMode(1);
        OutputStream out = mConnection.getOutputStream();
        out.write(1);
        IOException e = assertThrows(IOException.class, () -> out.write(1));
        // TODO(crbug.com/40286644): Consider whether we should be checking this in the first place.
        if (mTestRule.implementationUnderTest().equals(CronetImplementation.STATICALLY_LINKED)) {
            assertThat(e).isInstanceOf(NetworkException.class);
            NetworkException networkException = (NetworkException) e;
            assertThat(networkException.getErrorCode())
                    .isEqualTo(NetworkException.ERROR_CONNECTION_REFUSED);
        }

        // Make sure IOException is reported again when trying to read response
        // from the mConnection.
        e = assertThrows(IOException.class, mConnection::getResponseCode);
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
    public void testPost() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        OutputStream out = mConnection.getOutputStream();
        out.write(UPLOAD_DATA);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection)).isEqualTo(UPLOAD_DATA_STRING);
    }

    @Test
    @SmallTest
    public void testTransferEncodingHeaderSet() throws Exception {
        URL url = new URL(NativeTestServer.getEchoHeaderURL("Transfer-Encoding"));
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        OutputStream out = mConnection.getOutputStream();
        out.write(UPLOAD_DATA);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection)).isEqualTo("chunked");
    }

    @Test
    @SmallTest
    public void testPostOneMassiveWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        OutputStream out = mConnection.getOutputStream();
        byte[] largeData = TestUtil.getLargeData();
        out.write(largeData);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    public void testPostWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        OutputStream out = mConnection.getOutputStream();
        for (int i = 0; i < UPLOAD_DATA.length; i++) {
            out.write(UPLOAD_DATA[i]);
        }
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection)).isEqualTo(UPLOAD_DATA_STRING);
    }

    @Test
    @SmallTest
    public void testPostOneMassiveWriteWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        OutputStream out = mConnection.getOutputStream();
        byte[] largeData = TestUtil.getLargeData();
        for (int i = 0; i < largeData.length; i++) {
            out.write(largeData[i]);
        }
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    public void testPostWholeNumberOfChunks() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        int totalSize = UPLOAD_DATA.length * REPEAT_COUNT;
        int chunkSize = 18000;
        // Ensure total data size is a multiple of chunk size, so no partial
        // chunks will be used.
        assertThat(totalSize % chunkSize).isEqualTo(0);
        mConnection.setChunkedStreamingMode(chunkSize);
        OutputStream out = mConnection.getOutputStream();
        byte[] largeData = TestUtil.getLargeData();
        out.write(largeData);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    // Regression testing for crbug.com/618872.
    public void testOneMassiveWriteLargerThanInternalBuffer() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Use a super big chunk size so that it exceeds the UploadDataProvider
        // read buffer size.
        byte[] largeData = TestUtil.getLargeData();
        mConnection.setChunkedStreamingMode(largeData.length);
        OutputStream out = mConnection.getOutputStream();
        out.write(largeData);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }
}
