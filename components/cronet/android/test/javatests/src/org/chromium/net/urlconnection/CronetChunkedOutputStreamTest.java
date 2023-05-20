// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.net.CronetTestRule.getContext;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CompareDefaultWithCronet;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunCronetHttpURLConnection;
import org.chromium.net.NativeTestServer;
import org.chromium.net.NetworkException;

import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.ProtocolException;
import java.net.URL;

/**
 * Tests {@code getOutputStream} when {@code setChunkedStreamingMode} is enabled.
 * Tests annotated with {@code CompareDefaultWithCronet} will run once with the
 * default HttpURLConnection implementation and then with Cronet's
 * HttpURLConnection implementation. Tests annotated with
 * {@code OnlyRunCronetHttpURLConnection} only run Cronet's implementation.
 * See {@link CronetTestBase#runTest()} for details.
 */
@DoNotBatch(
        reason = "URL#setURLStreamHandlerFactory can be called at most once during JVM lifetime")
@RunWith(AndroidJUnit4.class)
public class CronetChunkedOutputStreamTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private static final String UPLOAD_DATA_STRING = "Nifty upload data!";
    private static final byte[] UPLOAD_DATA = UPLOAD_DATA_STRING.getBytes();
    private static final int REPEAT_COUNT = 100000;

    private CronetTestFramework mTestFramework;
    private HttpURLConnection mConnection;

    @Before
    public void setUp() throws Exception {
        mTestFramework = mTestRule.startCronetTestFramework();
        mTestRule.setStreamHandlerFactory(mTestFramework.mCronetEngine);
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @After
    public void tearDown() throws Exception {
        if (mConnection != null) {
            mConnection.disconnect();
        }
        mTestFramework.shutdownEngine();
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testGetOutputStreamAfterConnectionMade() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        try {
            mConnection.getOutputStream();
            fail();
        } catch (ProtocolException e) {
            // Expected.
        }
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testWriteAfterReadingResponse() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        OutputStream out = mConnection.getOutputStream();
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        try {
            out.write(UPLOAD_DATA);
            fail();
        } catch (IOException e) {
            // Expected.
        }
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testWriteAfterRequestFailed() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setChunkedStreamingMode(0);
        OutputStream out = mConnection.getOutputStream();
        out.write(UPLOAD_DATA);
        NativeTestServer.shutdownNativeTestServer();
        try {
            out.write(TestUtil.getLargeData());
            mConnection.getResponseCode();
            fail();
        } catch (IOException e) {
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
        // Set 1 byte as chunk size so internally Cronet will try upload when
        // 1 byte is filled.
        mConnection.setChunkedStreamingMode(1);
        try {
            OutputStream out = mConnection.getOutputStream();
            out.write(1);
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
    public void testPost() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
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
    @CompareDefaultWithCronet
    public void testTransferEncodingHeaderSet() throws Exception {
        URL url = new URL(NativeTestServer.getEchoHeaderURL("Transfer-Encoding"));
        mConnection = (HttpURLConnection) url.openConnection();
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
    @CompareDefaultWithCronet
    public void testPostOneMassiveWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
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
    @CompareDefaultWithCronet
    public void testPostWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
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
    @CompareDefaultWithCronet
    public void testPostOneMassiveWriteWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
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
    @CompareDefaultWithCronet
    public void testPostWholeNumberOfChunks() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
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
    @OnlyRunCronetHttpURLConnection
    // Regression testing for crbug.com/618872.
    public void testOneMassiveWriteLargerThanInternalBuffer() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
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
